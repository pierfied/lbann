import argparse
import datetime
import os
import os.path
import sys

from google.protobuf import text_format as txtf
import json
import numpy as np
import models.wae as molwae

import lbann
import lbann.contrib.launcher
import lbann.modules
import atom_models

def construct_lc_launcher_args():

    # defaults correspond to the settings needed for training on the moses dataset
    parser = argparse.ArgumentParser(prog="lbann ATOM WAE training")
    parser.add_argument("--partition", default=None)
    parser.add_argument("--account", default="hpcdl")
    parser.add_argument("--scheduler", type=str, default="slurm")
    parser.add_argument("--reservation", type=None)
    parser.add_argument(
        "--data-module-file",
        default="dataset.py",
        help="specifies the module that contains the logic for loading data",
    )
    parser.add_argument(
        "--data-config",
        default=os.path.join(
            os.path.abspath(os.path.dirname(__file__)), "zinc_data_config.json"
        ),
        help="path to a data config file that is used for the construction of python data reader",
    )
    parser.add_argument(
        "--time-limit",
        type=int,
        default=720,
        help="specified time limit in number of minutes",
    )
    parser.add_argument("--nodes", type=int, default=1)
    parser.add_argument("--job-name", default="atom_wae")
    parser.add_argument("--embedding-dim", type=int, default=None)
    parser.add_argument("--num-embeddings", type=int, default=None)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--z-dim", type=int, default=512, help="latent space dim")
    parser.add_argument("--g-mean", type=float, default=0.0, help="Gaussian mean")
    parser.add_argument("--g-std", type=float, default=1.0, help="Gaussian std")
    parser.add_argument(
        "--sgd-num-epochs", type=int, default=20,
        help="number of training epochs when training with SGD (ignored when training with LTFB)")
    parser.add_argument("--data-reader-prototext", default=None)
    parser.add_argument("--data-filedir", default=None)
    parser.add_argument("--data-filename", default=None)
    parser.add_argument("--pad-index", type=int, default=None)
    parser.add_argument("--sequence-length", type=int, default=None)
    parser.add_argument("--dump-weights-dir", type=str, default="weights")
    parser.add_argument("--dump-weights-interval", type=int, default=10)
    parser.add_argument("--dump-outputs-dir", type=str, default="outputs")
    parser.add_argument("--dump-outputs-interval", type=int, default=10)
    parser.add_argument("--dump-model-dir", type=str, default="models")
    parser.add_argument("--num-io-threads", type=int, default=11)
    parser.add_argument("--vocab", default=None)
    parser.add_argument(
        "--ltfb", type=bool, default=False, help="train with LTFB")
    parser.add_argument(
        "--ltfb-batch-interval", type=int, default=100,
        help="number of SGD steps between LTFB tournaments")
    parser.add_argument(
        "--ltfb-num-tournaments", type=int, default=100,
        help="number of LTFB tournaments")
    parser.add_argument("--warmup", type=bool, default=False)
    parser.add_argument("--lambda_", type=float, default=0.00157, help="weighting of adversarial loss")
    # these are specific to the Trainer object
    parser.add_argument(
        "--procs-per-trainer",
        type=int,
        default=0,
        help="number of processes to use per trainer",
    )
    parser.add_argument(
        "--lr",
        type=float,
        default=3e-4,
        help="optimizer learning rate to use for training",
    )
    return parser.parse_args()

# ==============================================
# Setup and launch experiment
# ==============================================

def construct_data_reader(run_args):
    """
    Construct Protobuf message for Python data reader.

    The Python data reader will import this Python file to access the
    sample access functions.

    """

    module_file = os.path.abspath(run_args.data_module_file)
    os.environ["DATA_CONFIG"] = os.path.abspath(run_args.data_config)
    os.environ["MAX_SEQ_LEN"] = str(run_args.sequence_length)
    #os.environ["DATA_PATH"] = "" #run_args.data_filename

    module_name = os.path.splitext(os.path.basename(module_file))[0]
    module_dir = os.path.dirname(module_file)

    print("module_name: {}\tmodule_dir: {}".format(module_name, module_dir))

    # Base data reader message
    message = lbann.reader_pb2.DataReader()

    # Training set data reader
    data_reader = message.reader.add()
    data_reader.name = "python"
    data_reader.role = "train"
    data_reader.shuffle = True
    data_reader.percent_of_data_to_use = 1.0
    data_reader.validation_percent = 0.1
    data_reader.tournament_percent = 0.1
    data_reader.python.module = module_name
    data_reader.python.module_dir = module_dir
    data_reader.python.sample_function = "get_sample"
    data_reader.python.num_samples_function = "num_samples"
    data_reader.python.sample_dims_function = "sample_dims"

    return message


def main():
    run_args = construct_lc_launcher_args()

    # add data_config data
    # and do not overwrite args if data_reader_prototext is enabled
    if os.path.isfile(run_args.data_config) and not run_args.data_reader_prototext:
        with open(run_args.data_config, "r") as f:
            config = json.load(f)
        for k, v in config.items():
            setattr(run_args, k, v)

    # Configure training algorithm
    if run_args.ltfb:
        tournament_type = lbann.RandomPairwiseExchange
        tournament = tournament_type(
            metric_strategies={'recon' : tournament_type.MetricStrategy.LOWER_IS_BETTER},
            exchange_strategy=tournament_type.ExchangeStrategy('checkpoint_binary'),
        )
        algo = lbann.LTFB(
            "ltfb",
            local_algo=lbann.BatchedIterativeOptimizer(
                "sgd",
                num_iterations=run_args.ltfb_batch_interval,
            ),
            metalearning=tournament,
            metalearning_steps=run_args.ltfb_num_tournaments,
        )
    else:
        algo = lbann.BatchedIterativeOptimizer(
            "sgd",
            num_iterations=run_args.ltfb_batch_interval,
            epoch_count=run_args.sgd_num_epochs,
        )

    # Configure trainer
    trainer = lbann.Trainer(
        mini_batch_size=run_args.batch_size,
        training_algo=algo,
    )

    # define data_reader
    if run_args.data_reader_prototext:
        print("Using data_reader_prototext")
        assert run_args.sequence_length is not None
        assert run_args.vocab is not None

        data_reader_proto = lbann.lbann_pb2.LbannPB()
        with open(run_args.data_reader_prototext, "r") as f:
            txtf.Merge(f.read(), data_reader_proto)
        data_reader = data_reader_proto.data_reader
    else:
        data_reader = construct_data_reader(run_args)

    if "LBANN_EXPERIMENT_DIR" in os.environ:
        work_dir = os.environ["LBANN_EXPERIMENT_DIR"]
    else:
        work_dir = os.path.join(os.getcwd())
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    experiment_dir = os.path.join(
        work_dir, "{}_{}".format(timestamp, run_args.job_name)
    )
    if not os.path.exists(experiment_dir):
        os.makedirs(experiment_dir)

    # model and optimizer
    model = atom_models.construct_atom_wae_model(pad_index=run_args.pad_index,
                                                 sequence_length=run_args.sequence_length,
                                                 embedding_size=run_args.embedding_dim,
                                                 dictionary_size=run_args.num_embeddings,
                                                 dump_outputs_dir=run_args.dump_outputs_dir,
                                                 z_dim=run_args.z_dim,
                                                 g_mean=run_args.g_mean,
                                                 g_std=run_args.g_std,
                                                 lambda_=run_args.lambda_,
                                                 dump_weights_dir=run_args.dump_weights_dir,
                                                 dump_weights_interval=run_args.dump_weights_interval,
                                                 dump_model_dir=run_args.dump_model_dir,
                                                 dump_outputs_interval=run_args.dump_outputs_interval,
                                                 warmup=run_args.warmup,
                                                 lr=run_args.lr,
                                                 batch_size=run_args.batch_size,
                                                 num_epochs=run_args.sgd_num_epochs
                                                 )
    opt = lbann.Adam(learn_rate=run_args.lr, beta1=0.9, beta2=0.99, eps=1e-8)

    # dump the config to the experiment_dir so that it can be used to load the model in pytorch (moses codebase)
    ppn = 4 if run_args.scheduler == "lsf" else 2
    print("args:\n" + str(run_args))
    if(run_args.scheduler == 'slurm'):
      import torch
      torch.save(run_args, "{}/{}_config.pt".format(experiment_dir, run_args.job_name))

    m_lbann_args=f"--vocab={run_args.vocab} --data_filedir={run_args.data_filedir} --data_filename_train={run_args.data_filename} --sequence_length={run_args.sequence_length}  --num_io_threads={run_args.num_io_threads}"
    if(run_args.data_reader_prototext):
      m_lbann_args = " ".join((m_lbann_args, " --use_data_store --preload_data_store "))
    if(run_args.procs_per_trainer):
      m_lbann_args = " ".join((m_lbann_args, f"--procs_per_trainer={run_args.procs_per_trainer}"))

    status = lbann.contrib.launcher.run(
        trainer,
        model,
        data_reader,
        opt,
        partition=run_args.partition,
        scheduler=run_args.scheduler,
        account=run_args.account,
        #reservation=run_args.reservation,
        time_limit=run_args.time_limit,
        nodes=run_args.nodes,
        procs_per_node=ppn,
        #batch_job = True,
        #setup_only = True,
        job_name=run_args.job_name,
        experiment_dir=experiment_dir,
        lbann_args = m_lbann_args,
        #turn on for tensor core
        environment = {
            'LBANN_USE_CUBLAS_TENSOR_OPS' : 1,
            'LBANN_USE_CUDNN_TENSOR_OPS' : 1,
        },
    )

    print("LBANN launcher status:\n" + str(status))


if __name__ == "__main__":
    sys.exit(main())
