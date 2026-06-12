// Copyright 2006 Google LLC
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Windows utility to dump the line number data from a pdb file to
// a text-based format that we can use from the minidump processor.

#ifdef HAVE_CONFIG_H
#include <config.h>  // Must come first
#endif

#include <cstdio>
#include <wchar.h>

#include <memory>
#include <string>

#include <direct.h>
#include <io.h>

#include "common/windows/pdb_source_line_writer.h"
#include "common/windows/pe_source_line_writer.h"

using google_breakpad::PDBSourceLineWriter;
using google_breakpad::PESourceLineWriter;
using google_breakpad::PDBModuleInfo;
using std::unique_ptr;
using std::wstring;

int usage(const wchar_t* self) {
  fprintf(stderr, "Usage: %ws [--pe] [--i] <file.[pdb|exe|dll]> [output_dir]\n",
          self);
  fprintf(stderr, "Options:\n");
  fprintf(stderr,
          "--pe:\tRead debugging information from PE file and do "
          "not attempt to locate matching PDB file.\n"
          "\tThis is only supported for PE32+ (64 bit) PE files.\n");
  fprintf(stderr,
          "--i:\tOutput INLINE/INLINE_ORIGIN record\n"
          "\tThis cannot be used with [--pe].\n");
  fprintf(stderr,
          "output_dir:\tOptional output directory. If specified, the symbol "
          "file will be written to\n"
          "\t<output_dir>/<module>/<debug_id>/<module>.sym instead of stdout.\n");
  return 1;
}

// Create a directory and all its parent directories.
// The path should point to a directory (not a file).
static bool CreateDirectoryRecursively(const wchar_t* dir_path) {
  wstring path_copy(dir_path);
  // Try to create the full directory path first.
  if (_wmkdir(path_copy.c_str()) == 0 || errno == EEXIST) {
    return true;
  }
  // Walk through the path creating each component.
  for (size_t i = 0; i < path_copy.size(); ++i) {
    if (path_copy[i] == L'\\' || path_copy[i] == L'/') {
      wchar_t saved = path_copy[i];
      path_copy[i] = L'\0';
      _wmkdir(path_copy.c_str());
      path_copy[i] = saved;
    }
  }
  return _wmkdir(path_copy.c_str()) == 0 || errno == EEXIST;
}

// Build the output symbol file path and create the directory structure.
// Returns the output file path on success, or empty string on failure.
static wstring CreateOutputPathAndDir(const wchar_t* output_dir,
                                      const wstring& debug_file,
                                      const wstring& debug_identifier) {
  // Determine the base filename (strip extension like .pdb or .dll for output).
  wstring base_file = debug_file;
  size_t dot_pos = base_file.find_last_of(L'.');
  if (dot_pos != wstring::npos) {
    base_file = base_file.substr(0, dot_pos);
  }

  // Build the directory path: <output_dir>/<debug_file>/<debug_id>/
  wstring dir_path = output_dir;
  dir_path += L"\\";
  dir_path += debug_file;
  dir_path += L"\\";
  dir_path += debug_identifier;

  // Build the full file path: <dir_path>/<base_file>.sym
  wstring out_path = dir_path;
  out_path += L"\\";
  out_path += base_file;
  out_path += L".sym";

  if (!CreateDirectoryRecursively(dir_path.c_str())) {
    fprintf(stderr, "Failed to create directory: %ws\n",
            dir_path.c_str());
    return L"";
  }
  return out_path;
}

int wmain(int argc, wchar_t** argv) {
  bool success = false;
  bool pe = false;
  bool handle_inline = false;
  int arg_index = 1;
  while (arg_index < argc && wcslen(argv[arg_index]) > 0 &&
         wcsncmp(L"--", argv[arg_index], 2) == 0) {
    if (wcscmp(L"--pe", argv[arg_index]) == 0) {
      pe = true;
    } else if (wcscmp(L"--i", argv[arg_index]) == 0) {
      handle_inline = true;
    }
    ++arg_index;
  }

  if ((pe && handle_inline) || arg_index == argc) {
    usage(argv[0]);
    return 1;
  }

  wchar_t* file_path = argv[arg_index];
  ++arg_index;

  // Determine if an output directory was provided.
  wchar_t* output_dir = nullptr;
  if (arg_index < argc && wcslen(argv[arg_index]) > 0) {
    output_dir = argv[arg_index];
  }

  if (pe) {
    PESourceLineWriter pe_writer(file_path);
    if (output_dir) {
      PDBModuleInfo info;
      if (!pe_writer.GetModuleInfo(&info)) {
        fprintf(stderr, "Failed to get module info.\n");
        return 1;
      }

      wstring out_path = CreateOutputPathAndDir(
          output_dir, info.debug_file, info.debug_identifier);
      if (out_path.empty()) {
        return 1;
      }

      FILE* out_file = _wfopen(out_path.c_str(), L"w");
      if (!out_file) {
        fprintf(stderr, "Failed to open FILE stream for: %ws\n",
                out_path.c_str());
        return 1;
      }

      // Need a new writer instance since the first was used for GetModuleInfo.
      PESourceLineWriter pe_writer2(file_path);
      success = pe_writer2.WriteSymbols(out_file);
      fclose(out_file);

      if (success) {
        fprintf(stderr, "Write PE symbols to: %ws\n", out_path.c_str());
      }
    } else {
      success = pe_writer.WriteSymbols(stdout);
    }
  } else {
    PDBSourceLineWriter pdb_writer(handle_inline);
    if (!pdb_writer.Open(wstring(file_path), PDBSourceLineWriter::ANY_FILE)) {
      fprintf(stderr, "Open failed.\n");
      return 1;
    }

    if (output_dir) {
      PDBModuleInfo info;
      if (!pdb_writer.GetModuleInfo(&info)) {
        fprintf(stderr, "Failed to get module info.\n");
        return 1;
      }

      wstring out_path = CreateOutputPathAndDir(
          output_dir, info.debug_file, info.debug_identifier);
      if (out_path.empty()) {
        return 1;
      }

      FILE* out_file = _wfopen(out_path.c_str(), L"w");
      if (!out_file) {
        fprintf(stderr, "Failed to open FILE stream for: %ws\n",
                out_path.c_str());
        return 1;
      }

      success = pdb_writer.WriteSymbols(out_file);
      fclose(out_file);

      if (success) {
        fprintf(stderr, "Write PDB symbols to: %ws\n", out_path.c_str());
      }
    } else {
      success = pdb_writer.WriteSymbols(stdout);
    }
  }

  if (!success) {
    fprintf(stderr, "WriteSymbols failed.\n");
    return 1;
  }

  return 0;
}