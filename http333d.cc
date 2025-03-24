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

#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <list>
#include <filesystem>
#include <fstream>

#include "./ServerSocket.h"
#include "./HttpServer.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::string;

// min port number to check for
static const int kPortNum = 1024;

// Print out program usage, and exit() with EXIT_FAILURE.
static void Usage(char *prog_name);

// Parse command-line arguments to get port, path, and indices to use
// for your http333d server.
//
// Params:
// - argc: number of argumnets
// - argv: array of arguments
// - port: output parameter returning the port number to listen on
// - path: output parameter returning the directory with our static files
// - indices: output parameter returning the list of index file names
//
// Calls Usage() on failure. Possible errors include:
// - path is not a readable directory
// - index file names are readable
static void GetPortAndPath(int argc,
                           char **argv,
                           uint16_t *const port,
                           string *const path,
                           list<string> *const indices);

int main(int argc, char **argv) {
  // Print out welcome message.
  cout << "Welcome to http333d, the UW CSE333 web server!" << endl;
  cout << "  Copyright 2012 Steven Gribble" << endl;
  cout << "  http://www.cs.washington.edu/homes/gribble" << endl;
  cout << endl;
  cout << "initializing:" << endl;
  cout << "  parsing port number and static files directory..." << endl;

  // Ignore the SIGPIPE signal, otherwise we'll crash out if a client
  // disconnects unexpectedly.
  signal(SIGPIPE, SIG_IGN);

  // Get the port number and list of index files.
  uint16_t port_num;
  string static_dir;
  list<string> indices;
  GetPortAndPath(argc, argv, &port_num, &static_dir, &indices);
  cout << "    port: " << port_num << endl;
  cout << "    static file path: " << static_dir << endl;

  // Run the server.
  hw4::HttpServer hs(port_num, static_dir, indices);
  if (!hs.Run()) {
    cerr << "  server failed to run!?" << endl;
  }

  cout << "server completed!  Exiting." << endl;
  return EXIT_SUCCESS;
}


static void Usage(char *prog_name) {
  cerr << "Usage: " << prog_name << " port staticfiles_directory indices+";
  cerr << endl;
  exit(EXIT_FAILURE);
}

static void GetPortAndPath(int argc,
                           char **argv,
                           uint16_t *const port,
                           string *const path,
                           list<string> *const indices) {
  // Here are some considerations when implementing this function:
  // - There is a reasonable number of command line arguments
  // - The port number is reasonable
  // - The path (i.e., argv[2]) is a readable directory
  // - You have at least 1 index, and all indices are readable file

  // STEP 1:

  // check for reasonable number of args
  // executable name, port, path, and at least 1 index
  if (argc < 4) {
    Usage(argv[1]);
  }

  // check if port is a reasonable number
  if (std::stoi(argv[1]) < kPortNum) {
    cerr << "Port should be >= 1024" << endl;
    Usage(argv[0]);
  }
  *port = std::stoi(argv[1]);

  // check directory path is an actual readable directory
  if (!std::filesystem::is_directory(argv[2]) ||
      !std::filesystem::exists(argv[2])) {
    cerr << "Not a valid readable directory" << endl;
    Usage(argv[0]);
  }
  *path = argv[2];

  // go through remaining args to check for valid indices
  for (int i = 3; i < argc; i++) {
    std::ifstream file(argv[i], std::fstream::in);

    // check if the file exists and is readable
    if (!std::filesystem::exists(argv[i]) || file.bad()) {
      cerr << "index " << std::to_string(i - 3)
          << " is not a valid index" << endl;

      Usage(argv[0]);
    }

    indices->push_back(argv[i]);
  }
}

