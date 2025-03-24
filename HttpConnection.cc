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

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

#include <iostream>

using std::map;
using std::string;
using std::vector;

namespace hw4 {

using namespace boost;

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;
static const size_t kToRead = 1024;

bool HttpConnection::GetNextRequest(HttpRequest *const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:
  while (buffer_.find(kHeaderEnd) == string::npos) {
    // read in data
    unsigned char temp_read[kToRead + 1];
    int read = WrappedRead(fd_, temp_read, kToRead);

    if (read == -1) {
      return false;
    } else if (read == 0) {
      break;
    } else {
      // add the read data to our buffer once everything is good
      temp_read[read] = '\0';
      buffer_ += string(reinterpret_cast<char*>(temp_read), read);
    }
  }

  size_t index = buffer_.find(kHeaderEnd);
  // no completed request was found
  if (index == string::npos) {
    return false;
  }

  // parse the request + header end
  *request = ParseRequest(buffer_.substr(0, index + kHeaderEndLen));

  // have buffer_ start at the beginning of the next request, if there is one
  buffer_ = buffer_.substr(index + kHeaderEndLen, buffer_.length());

  return true;
}

bool HttpConnection::WriteResponse(const HttpResponse &response) const {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());
  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string &request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:

  // make the whole request lowercase
  string r = request.substr(0, request.length());
  boost::to_lower(r);

  vector<string> lines;
  split(lines, r, is_any_of("\r\n"), token_compress_on);

  // extract the parts of the first line and check request validity
  // split the first line into the seperate parts
  vector<string> components;
  split(components, lines[0], is_any_of(" "), token_compress_on);

  // store uri
  req.set_uri(components[1]);

  for (uint32_t i = 1; i < lines.size(); i++) {
    // extract the name and value
    vector<string> line;
    split(line, lines[i], is_any_of(":"), token_compress_on);

    // check if malformed
    if (line.size() != 2) {
      continue;
    }

    // remove any white space
    trim(line[0]);
    trim(line[1]);

    // add the header into req
    req.AddHeader(line[0], line[1]);
  }

  return req;
}

}  // namespace hw4
