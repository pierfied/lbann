#!/bin/sh

DIRNAME=`dirname $0`
#Set Script Name variable
SCRIPT=`basename ${0}`

# Figure out which cluster we are on
CLUSTER=`hostname | sed 's/\([a-zA-Z][a-zA-Z]*\)[0-9]*/\1/g'`

RUN="srun"

#Set fonts for Help.
NORM=`tput sgr0`
BOLD=`tput bold`
REV=`tput smso`

#Help function
function HELP {
  echo -e \\n"Help documentation for ${BOLD}${SCRIPT}.${NORM}"\\n
  echo -e "${REV}Basic usage:${NORM} ${BOLD}$SCRIPT -t <training set size> -e <epochs> -v <validation set size>${NORM}"\\n
  echo "Command line switches are optional. The following switches are recognized."
  echo "${REV}-a${NORM} <val> --Sets the ${BOLD}activation type${NORM}. Default is ${BOLD}${ACT}${NORM}."
  echo "${REV}-b${NORM} <val> --Sets the ${BOLD}mini-batch size${NORM}. Default is ${BOLD}${MB_SIZE}${NORM}."
  echo "${REV}-c${NORM}       --(CHEAT) Test / validate with the ${BOLD}training data${NORM}. Default is ${BOLD}${TEST_W_TRAIN_DATA}${NORM}."
  echo "${REV}-d${NORM}       --Sets the ${BOLD}debug mode${NORM}."
  echo "${REV}-e${NORM} <val> --Sets the ${BOLD}number of epochs${NORM}. Default is ${BOLD}${EPOCHS}${NORM}."
  echo "${REV}-f${NORM} <val> --Path to the ${BOLD}datasets${NORM}. Default is ${BOLD}${ROOT_DATASET_DIR}${NORM}."  
  echo "${REV}-i${NORM} <val> --Sets the ${BOLD}parallel I/O limit${NORM}. Default is ${BOLD}${PARIO}${NORM}."
  echo "${REV}-j${NORM} <val> --Sets the ${BOLD}learning rate decay${NORM}. Default is ${BOLD}${LR_DECAY}${NORM}."
  echo "${REV}-k${NORM} <val> --Sets the ${BOLD}number of processes per model${NORM}. 0 for one model."
  echo "${REV}-l${NORM} <val> --Determines if the model is ${BOLD}loaded${NORM}. Default is ${BOLD}${LOAD_MODEL}${NORM}."
  echo "${REV}-m${NORM} <val> --Sets the ${BOLD}mode${NORM}. Default is ${BOLD}${MODE}${NORM}."
  echo "${REV}-n${NORM} <val> --Sets the ${BOLD}network topology${NORM}. Default is ${BOLD}${NETWORK}${NORM}."
  echo "${REV}-o${NORM} <val> --Sets the ${BOLD}output directory${NORM}. Default is ${BOLD}${OUTPUT_DIR}${NORM}."
  echo "${REV}-p${NORM} <val> --Sets the ${BOLD}input parameter directory${NORM}. Default is ${BOLD}${PARAM_DIR}${NORM}."
  echo "${REV}-q${NORM} <val> --Sets the ${BOLD}learning rate method${NORM}. Default is ${BOLD}${LRM}${NORM}."
  echo "${REV}-r${NORM} <val> --Sets the ${BOLD}inital learning rate${NORM}. Default is ${BOLD}${LR}${NORM}."
  echo "${REV}-s${NORM} <val> --Determines if the model is ${BOLD}saved${NORM}. Default is ${BOLD}${SAVE_MODEL}${NORM}."
  echo "${REV}-t${NORM} <val> --Sets the number of ${BOLD}training samples${NORM}. Default is ${BOLD}${TRAINING_SAMPLES}${NORM}."
  echo "${REV}-u${NORM}       --Use the ${BOLD}Lustre filesystem${NORM} directly. Default is ${BOLD}${USE_LUSTRE_DIRECT}${NORM}."
  echo "${REV}-v${NORM} <val> --Sets the number of ${BOLD}validation samples${NORM}. Default is ${BOLD}${VALIDATION_SAMPLES}${NORM}."
  echo "${REV}-w${NORM} <val> --Sets the ${BOLD}summary output directory${NORM}."
  echo "${REV}-x${NORM} <val> --Sets the type of ${BOLD}intermodel communication${NORM}."
  echo "${REV}-z${NORM} <val> --Sets the ${BOLD}tasks per node${NORM}. Default is ${BOLD}${TASKS_PER_NODE}${NORM}."
  echo -e "${REV}-h${NORM}    --Displays this help message. No further functions are performed."\\n
  exit 1
}

TASKS_PER_NODE=12
export SLURM_NNODES=$SLURM_JOB_NUM_NODES

# Look for the binary in the cluster specific build directory
BINDIR="${DIRNAME}/../build/gnu.${CLUSTER}.llnl.gov${DEBUGDIR}/model_zoo"

#add whatever is on the command line to options
OPTS=""
for v in "$@"; do
  OPTS="$OPTS $v"
done

TASKS=$((${SLURM_JOB_NUM_NODES} * ${SLURM_CPUS_ON_NODE}))
if [ ${TASKS} -gt 384 ]; then
TASKS=384
fi
LBANN_TASKS=$((${SLURM_NNODES} * ${TASKS_PER_NODE}))

CMD="${RUN} -n${LBANN_TASKS}  \
  --ntasks-per-node=${TASKS_PER_NODE} \
  ${BINDIR}/lbann \
  --model=../model_zoo/prototext/model_mnist_multi.prototext \
  --reader=../model_zoo/prototext/data_reader_mnist.prototext \
  --optimizer=../model_zoo/prototext/opt_adagrad.prototext \
  $OPTS"

echo ${CMD}
${CMD}
