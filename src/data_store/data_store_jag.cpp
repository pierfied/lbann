////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
//
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_store/data_store_jag.hpp"

#ifdef LBANN_HAS_CONDUIT

#include "lbann/data_readers/data_reader_jag_conduit.hpp"
#include "lbann/utils/exception.hpp"
#include "lbann/utils/options.hpp"
#include "lbann/utils/timer.hpp"
#include "lbann/utils/file_utils.hpp" // for add_delimiter()

#include <unordered_set>

#undef DEBUG
#define DEBUG

namespace lbann {

data_store_jag::data_store_jag(
  generic_data_reader *reader, model *m) :
  generic_data_store(reader, m) {
  set_name("data_store_jag");
}

data_store_jag::~data_store_jag() {
}

void data_store_jag::setup() {
  double tm1 = get_time();
  std::stringstream err;

  if (m_master) {
    std::cout << "starting data_store_jag::setup() for role: " 
              << m_reader->get_role() << "\n"
              << "calling generic_data_store::setup()\n";
  }
  generic_data_store::setup();
  m_jag_reader = dynamic_cast<data_reader_jag_conduit*>(m_reader);
  if (m_jag_reader == nullptr) {
    LBANN_ERROR(" dynamic_cast<data_reader_jag_conduit*>(m_reader) failed");
  }

/*
  m_send_buffer.resize(m_np);
  m_send_buffer_2.resize(m_np);
  m_send_requests.resize(m_np);
  m_recv_requests.resize(m_np);
  m_status.resize(m_np);
  m_outgoing_msg_sizes.resize(m_np);
  m_incoming_msg_sizes.resize(m_np);
  m_recv_buffer.resize(m_np);
*/

  // builds map: shuffled_index subscript -> owning proc
//  build_index_owner();

  if (! m_in_memory) {
    LBANN_ERROR("out-of-memory mode for data_store_jag has not been implemented");
  } 
  
  else {
    if (m_master) std::cout << "calling get_minibatch_index_vector\n";
    get_minibatch_index_vector();
    
    if (m_master) std::cout << "calling exchange_mb_indices()\n";
    exchange_mb_indices();
  }
  if (m_master) {
    std::cout << "TIME for data_store_jag setup: " << get_time() - tm1 << "\n";
  }
}

#if 0
void data_store_jag::get_indices(std::unordered_set<int> &indices, int p) {
  indices.clear();
  std::vector<int> &v = m_all_minibatch_indices[p];
  for (auto t : v) {
    indices.insert((*m_shuffled_indices)[t]);
  }
}
#endif


void data_store_jag::exchange_data() {
  LBANN_ERROR("starting data_store_jag::exchange_data for epoch: " + std::to_string(m_epoch) + "; not yet implemented; m_data.size: " + std::to_string(m_data.size()));

#if 0
  double tm1 = get_time();

  //========================================================================
  //build map: proc -> global indices that P_x needs for this epoch, and
  //                   which I own
  std::vector<std::unordered_set<int>> proc_to_indices(m_np);
  for (size_t p=0; p<m_all_minibatch_indices.size(); p++) {
    for (auto idx : m_all_minibatch_indices[p]) {
      int index = (*m_shuffled_indices)[idx];
      if (m_my_datastore_indices.find(index) != m_my_datastore_indices.end()) {
        proc_to_indices[p].insert(index);
      }
    }
  }

  if (m_master) std::cout << "exchange_data; built map\n";

  //========================================================================
  //part 1: exchange the sizes of the data
  
  for (size_t j=0; j<m_send_buffer.size(); j++) {
    m_send_buffer[j].reset();
  }
  int my_first = m_sample_ownership[m_rank];
  for (int p=0; p<m_np; p++) {
    for (auto idx : proc_to_indices[p]) {
      int local_idx = idx - my_first;
      if (local_idx >= (int)m_data.size()) {
        throw lbann_exception(std::string{} + __FILE__ + " " + std::to_string(__LINE__) + " :: local index: " + std::to_string(local_idx) + " is >= m_data.size(): " + std::to_string(m_data.size()));
      }
      if (local_idx < 0) {
        throw lbann_exception(std::string{} + __FILE__ + " " + std::to_string(__LINE__) + " :: local index: " + std::to_string(local_idx) + " is < 0");
      }

      m_send_buffer[p][std::to_string(idx)] = m_data[local_idx];
    }

    build_node_for_sending(m_send_buffer[p], m_send_buffer_2[p]);

    m_outgoing_msg_sizes[p] = m_send_buffer_2[p].total_bytes_compact();
    MPI_Isend((void*)&m_outgoing_msg_sizes[p], 1, MPI_INT, p, 0, MPI_COMM_WORLD, &m_send_requests[p]);
  }

  //start receives for sizes of the data
  for (int p=0; p<m_np; p++) {
    MPI_Irecv((void*)&m_incoming_msg_sizes[p], 1, MPI_INT, p, 0, MPI_COMM_WORLD, &m_recv_requests[p]);
  }

  // wait for all msgs to complete
  MPI_Waitall(m_np, m_send_requests.data(), m_status.data());
  MPI_Waitall(m_np, m_recv_requests.data(), m_status.data());

  //========================================================================
  //part 2: exchange the actual data
  
  // start sends for outgoing data
  for (int p=0; p<m_np; p++) {
  if (m_master && p==0) {
  }
    const void *s = m_send_buffer_2[p].data_ptr();
    MPI_Isend(s, m_outgoing_msg_sizes[p], MPI_BYTE, p, 1, MPI_COMM_WORLD, &m_send_requests[p]);
  }

  m_comm->global_barrier();
  if (m_master) std::cout << "started sends for outgoing data\n";
  m_comm->global_barrier();

  // start recvs for incoming data
  for (int p=0; p<m_np; p++) {
    m_recv_buffer[p].set(conduit::DataType::uint8(m_incoming_msg_sizes[p]));
    MPI_Irecv(m_recv_buffer[p].data_ptr(), m_incoming_msg_sizes[p], MPI_BYTE, p, 1, MPI_COMM_WORLD, &m_recv_requests[p]);
  }

  m_comm->global_barrier();
  if (m_master) std::cout << "started recvs for incoming data\n";
  m_comm->global_barrier();


  // wait for all msgs to complete
  MPI_Waitall(m_np, m_send_requests.data(), m_status.data());
  MPI_Waitall(m_np, m_recv_requests.data(), m_status.data());
if (m_master) std::cout << "finished waiting!\n\n";

  //========================================================================
  //part 3: construct the Nodes needed by me for the current minibatch

  for (int p=0; p<m_np; p++) {
    conduit::uint8 *n_buff_ptr = (conduit::uint8*)m_recv_buffer[p].data_ptr();
    conduit::Node n_msg;
    n_msg["schema_len"].set_external((conduit::int64*)n_buff_ptr);
    n_buff_ptr +=8;
    n_msg["schema"].set_external_char8_str((char*)(n_buff_ptr));
    conduit::Schema rcv_schema;
    conduit::Generator gen(n_msg["schema"].as_char8_str());
    gen.walk(rcv_schema);
    n_buff_ptr += n_msg["schema"].total_bytes_compact();
    n_msg["data"].set_external(rcv_schema,n_buff_ptr);
    m_minibatch_data[p].update(n_msg["data"]);
  }

  if (m_master) std::cout << "data_store_jag::exchange_data time: " << get_time() - tm1 << "\n";
#endif
}

void data_store_jag::set_conduit_node(int data_id, conduit::Node &node) {
  if (m_data.find(data_id) != m_data.end()) {
    LBANN_ERROR("duplicate data_id: " + std::to_string(data_id) + " in data_store_jag::set_conduit_node");
  }
  m_data[data_id] = node;
}
  
const conduit::Node & data_store_jag::get_conduit_node(int data_id, bool any_node) const {
  if (any_node) {
    LBANN_ERROR("data_store_jag::get_conduit_node called with any_node = true; this is not yet functional; please contact Dave Hysom");
  }
  
  std::unordered_map<int, conduit::Node>::const_iterator t = m_minibatch_data.find(data_id);
  if (t == m_minibatch_data.end()) {
    LBANN_ERROR("failed to find data_id: " + std::to_string(data_id) + " in m_minibatch_data; m_minibatch_data.size: " + std::to_string(m_minibatch_data.size()));
  }

  return t->second;
}

#if 0
void data_store_jag::build_node_for_sending(const conduit::Node &node_in, conduit::Node &node_out) {
  node_out.reset();
  conduit::Schema s_data_compact;
  if( node_in.is_compact() && node_in.is_contiguous()) {
    s_data_compact = node_in.schema();
  } else {
    node_in.schema().compact_to(s_data_compact);
  }

  std::string snd_schema_json = s_data_compact.to_json();
if (m_master)  {
  std::cout << "XXXX:\n";
  std::cout << snd_schema_json << "\n";
  }

  conduit::Schema s_msg;
  s_msg["schema_len"].set(conduit::DataType::int64());
  s_msg["schema"].set(conduit::DataType::char8_str(snd_schema_json.size()+1));
  s_msg["data"].set(s_data_compact);

  conduit::Schema s_msg_compact;
  s_msg.compact_to(s_msg_compact);
  conduit::Node n_msg(s_msg_compact);
  conduit::Node tmp;
  tmp["schema"].set(snd_schema_json);
  tmp["data"].update(node_in);
static bool doit = true;
if (m_master && doit) {
  std::cout << "1. RRRRRRRRRRRR\n";
  conduit::Node n3 = tmp["schema"];
  n3.print();
  std::cout << "WWWWWWWWWWWWWWWWWWWWWWWWWWWWW\n\n";
}
  tmp.compact_to(node_out);
if (m_master && doit) {
  std::cout << "2. RRRRRRRRRRRR\n";
  conduit::Node n3 = node_out["schema"];
  n3.print();
  std::cout << "WWWWWWWWWWWWWWWWWWWWWWWWWWWWW\n\n";
  doit = false;
}
}
#endif

void data_store_jag::build_node_for_sending(const conduit::Node &node_in, conduit::Node &node_out) {
#if 0
  conduit::Schema s_data_compact;
  if( node_in.is_compact() && node_in.is_contiguous()) {
    s_data_compact = node_in.schema();
  } else {
    node_in.schema().compact_to(s_data_compact);
  }

  std::string snd_schema_json = s_data_compact.to_json();
/*
if (m_master)  {
  std::cout << "XXXX:\n";
  std::cout << snd_schema_json << "\n";
  }
*/

  conduit::Schema s_msg;
  s_msg["schema_len"].set(conduit::DataType::int64());
  s_msg["schema"].set(conduit::DataType::char8_str(snd_schema_json.size()+1));
  s_msg["data"].set(s_data_compact);

  conduit::Schema s_msg_compact;
  s_msg.compact_to(s_msg_compact);
  //conduit::Node n_msg(s_msg_compact);
  node_out.reset();
  node_out.set(s_msg_compact);
  node_out["schema"].set(snd_schema_json);
  node_out["data"].update(node_in);
/*
static bool doit = true;
if (m_master && doit) {
  std::cout << "1. RRRRRRRRRRRR\n";
  conduit::Node n3 = node_out["schema"];
  n3.print();
  std::cout << "WWWWWWWWWWWWWWWWWWWWWWWWWWWWW\n\n";
}
*/
/*
  node_in.compact_to(node_out);
if (m_master && doit) {
  std::cout << "2. RRRRRRRRRRRR\n";
  conduit::Node n3 = node_out["schema"];
  n3.print();
  std::cout << "WWWWWWWWWWWWWWWWWWWWWWWWWWWWW\n\n";
  doit = false;
}
*/
#endif
}

// fills in m_owner: std::unordered_map<int, int> m_owner
// which maps an index to the processor that owns the associated data
void data_store_jag::build_index_owner() {
#if 0
  //todo: should be performed by P_0 then bcast; for now we'll
  //      have all procs do it
  
  std::stringstream err;

  // get filename for sample list
  m_base_dir = add_delimiter(m_data_reader->get_file_dir());
  m_sample_list_fn = m_base_dir + m_data_reader->get_data_index_list();

  // get filename for mapping file
  m_mapping_fn = m_base_dir + m_data_reader->get_local_file_dir();

  //file_owners[i] contains the counduit filenames that P_i owns
  std::vector<std::vector<std::string>> file_owners(m_np);

  // sample_counts[i][j] contains the number of valid samples
  // in the conduit file: file_owners[[i][j]
  std::vector<std::vector<int>> sample_counts(m_np);

  std::ifstream in(sample_list_file);
  if (!in) {
    err << "failed to open " << sample_list_file << " for reading";
    LBANN_ERROR(err);
  }


  in.close()
#endif
}


}  // namespace lbann

#endif //#ifdef LBANN_HAS_CONDUIT
