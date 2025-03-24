/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Autumn Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./QueryProcessor.h"

#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <map>
#include <utility>

extern "C" {
  #include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;
using std::map;

namespace hw3 {

QueryProcessor::QueryProcessor(const list<string>& index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader* [array_len_];
  itr_array_ = new IndexTableReader* [array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int     rank;    // The rank of the result so far.
} IdxQueryResult;

vector<QueryProcessor::QueryResult>
QueryProcessor::ProcessQuery(const vector<string>& query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)
  vector<QueryProcessor::QueryResult> final_result;

  // go through each document and check for the first query
  vector<vector<QueryProcessor::QueryResult>> all_results;
  for (int q = 0; q < static_cast<int>(query.size()); q++) {
    // create a new vector for results for this query specifically
    all_results.push_back(vector<QueryProcessor::QueryResult>());

    for (int i = 0; i < array_len_; i++) {
      DocIDTableReader *ditr = itr_array_[i]->LookupWord(query[q]);

      // if nothing is found, move on to next document
      if (ditr == nullptr) {
        continue;
      }

      list<DocIDElementHeader> doc_ids = ditr->GetDocIDList();

      // don't need docidtable reader anymore
      delete ditr;

      // put the results for the current query word in this index in list
      for (DocIDElementHeader el : doc_ids) {
        // get document's name
        string file_name;
        dtr_array_[i]->LookupDocID(el.doc_id, &file_name);

        QueryResult result;
        result.rank = el.num_positions;
        result.document_name = file_name;
        all_results[q].push_back(result);
      }
    }
  }

  // go through the results for each query and check for common documents
  if (!all_results.empty()) {
    // create a map to keep track of the combined rank for each document
    map<string, int> doc_ranks;

    // initialize the map with the results of the first query
    for (const auto& result : all_results[0]) {
      doc_ranks[result.document_name] = result.rank;
    }

    // for each query, check if the document is in the map
    for (size_t q = 1; q < all_results.size(); q++) {
      map<string, int> temp_map;

      for (const auto& result : all_results[q]) {
        if (doc_ranks.find(result.document_name) != doc_ranks.end()) {
          // this doc was found, add to rank
        temp_map[result.document_name] =
            doc_ranks[result.document_name] + result.rank;
        }
      }

      // update the doc_ranks map to only include documents
      // present in the current query
      doc_ranks = std::move(temp_map);
    }

    // convert the final map to the final_result vector
    for (const auto& pair : doc_ranks) {
      QueryProcessor::QueryResult result;
      result.document_name = pair.first;
      result.rank = pair.second;
      final_result.push_back(result);
    }
  }

  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

}  // namespace hw3
