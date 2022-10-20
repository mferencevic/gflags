// Copyright (c) 1999, Google Inc.
// All rights reserved.
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
//     * Neither the name of Google Inc. nor the names of its
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

// ---
//
// Revamped and reorganized by Craig Silverstein
//
// This file contains code for handling the 'reporting' flags.  These
// are flags that, when present, cause the program to report some
// information and then exit.  --help and --version are the canonical
// reporting flags, but we also have flags like --help-xml, etc.
//
// There's only one function that's meant to be called externally:
// HandleCommandLineHelpFlags().  (Well, actually, ShowUsageWithFlags(),
// and DescribeOneFlag() can be called externally too, but there's little
// need for it.)  These are all declared in the main gflags.h header file.
//
// HandleCommandLineHelpFlags() will check what 'reporting' flags have
// been defined, if any -- the "help" part of the function name is a
// bit misleading -- and do the relevant reporting.  It should be
// called after all flag-values have been assigned, that is, after
// parsing the command-line.

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cassert>
#include <algorithm>
#include <string>
#include <vector>

#include "config.h"
#include "gflags/gflags.h"
#include "util.h"


// The 'reporting' flags.  They all call gflags_exitfunc().
DEFINE_bool  (help,        false, "show help on all flags [tip: all flags can have two dashes]");
DEFINE_bool  (help_xml,     false, "produce an xml version of help");
DEFINE_bool  (version,     false, "show version and build info and exit");


namespace GFLAGS_NAMESPACE {


using std::string;
using std::vector;


// --------------------------------------------------------------------
// DescribeOneFlag()
// DescribeOneFlagInXML()
//    Routines that pretty-print info about a flag.  These use
//    a CommandLineFlagInfo, which is the way the gflags
//    API exposes static info about a flag.
// --------------------------------------------------------------------

static const int kLineLength = 80;

static void AddString(const string& s,
                      string* final_string, int* chars_in_line) {
  const int slen = static_cast<int>(s.length());
  if (*chars_in_line + 1 + slen >= kLineLength) {  // < 80 chars/line
    *final_string += "\n        ";
    *chars_in_line = 8;
  } else {
    *final_string += " ";
    *chars_in_line += 1;
  }
  *final_string += s;
  *chars_in_line += slen;
}

static string PrintStringFlagsWithQuotes(const CommandLineFlagInfo& flag,
                                         const string& text, bool current) {
  const char* c_string = (current ? flag.current_value.c_str() :
                          flag.default_value.c_str());
  if (strcmp(flag.type.c_str(), "string") == 0) {  // add quotes for strings
    return StringPrintf("%s: \"%s\"", text.c_str(), c_string);
  } else {
    return StringPrintf("%s: %s", text.c_str(), c_string);
  }
}

// Create a descriptive string for a flag.
// Goes to some trouble to make pretty line breaks.
string DescribeOneFlag(const CommandLineFlagInfo& flag) {
  string main_part;
  string flag_name = flag.name;
  std::replace(flag_name.begin(), flag_name.end(), '_', '-');
  SStringPrintf(&main_part, "    --%s (%s)",
                flag_name.c_str(),
                flag.description.c_str());
  const char* c_string = main_part.c_str();
  int chars_left = static_cast<int>(main_part.length());
  string final_string;
  int chars_in_line = 0;  // how many chars in current line so far?
  while (1) {
    assert(static_cast<size_t>(chars_left)
            == strlen(c_string));  // Unless there's a \0 in there?
    const char* newline = strchr(c_string, '\n');
    if (newline == NULL && chars_in_line+chars_left < kLineLength) {
      // The whole remainder of the string fits on this line
      final_string += c_string;
      chars_in_line += chars_left;
      break;
    }
    if (newline != NULL && newline - c_string < kLineLength - chars_in_line) {
      int n = static_cast<int>(newline - c_string);
      final_string.append(c_string, n);
      chars_left -= n + 1;
      c_string += n + 1;
    } else {
      // Find the last whitespace on this 80-char line
      int whitespace = kLineLength-chars_in_line-1;  // < 80 chars/line
      while ( whitespace > 0 && !isspace(c_string[whitespace]) ) {
        --whitespace;
      }
      if (whitespace <= 0) {
        // Couldn't find any whitespace to make a line break.  Just dump the
        // rest out!
        final_string += c_string;
        chars_in_line = kLineLength;  // next part gets its own line for sure!
        break;
      }
      final_string += string(c_string, whitespace);
      chars_in_line += whitespace;
      while (isspace(c_string[whitespace]))  ++whitespace;
      c_string += whitespace;
      chars_left -= whitespace;
    }
    if (*c_string == '\0')
      break;
    StringAppendF(&final_string, "\n        ");
    chars_in_line = 8;
  }

  // Append data type
  AddString(string("type: ") + flag.type, &final_string, &chars_in_line);
  // The listed default value will be the actual default from the flag
  // definition in the originating source file, unless the value has
  // subsequently been modified using SetCommandLineOptionWithMode() with mode
  // SET_FLAGS_DEFAULT, or by setting FLAGS_foo = bar before ParseCommandLineFlags().
  AddString(PrintStringFlagsWithQuotes(flag, "default", false), &final_string,
            &chars_in_line);
  if (!flag.is_default) {
    AddString(PrintStringFlagsWithQuotes(flag, "currently", true),
              &final_string, &chars_in_line);
  }

  StringAppendF(&final_string, "\n");
  return final_string;
}

// Simple routine to xml-escape a string: escape & and < only.
static string XMLText(const string& txt) {
  string ans = txt;
  for (string::size_type pos = 0; (pos = ans.find("&", pos)) != string::npos; )
    ans.replace(pos++, 1, "&amp;");
  for (string::size_type pos = 0; (pos = ans.find("<", pos)) != string::npos; )
    ans.replace(pos++, 1, "&lt;");
  return ans;
}

static void AddXMLTag(string* r, const char* tag, const string& txt) {
  StringAppendF(r, "<%s>%s</%s>", tag, XMLText(txt).c_str(), tag);
}


static string DescribeOneFlagInXML(const CommandLineFlagInfo& flag) {
  // The file and flagname could have been attributes, but default
  // and meaning need to avoid attribute normalization.  This way it
  // can be parsed by simple programs, in addition to xml parsers.
  string r("<flag>");
  AddXMLTag(&r, "name", flag.name);
  AddXMLTag(&r, "meaning", flag.description);
  AddXMLTag(&r, "default", flag.default_value);
  AddXMLTag(&r, "current", flag.current_value);
  AddXMLTag(&r, "type", flag.type);
  r += "</flag>";
  return r;
}

// --------------------------------------------------------------------
// ShowUsageWithFlags()
// ShowXMLOfFlags()
//    These routines variously expose the registry's list of flag
//    values.  ShowUsage*() prints the flag-value information
//    to stdout in a user-readable format (that's what --help uses).
//    ShowXMLOfFlags() prints the flag-value information to stdout
//    in a machine-readable format.  In all cases, the flags are
//    sorted: by flagname.
// --------------------------------------------------------------------

static const char* Basename(const char* filename) {
  const char* sep = strrchr(filename, PATH_SEPARATOR);
  return sep ? sep + 1 : filename;
}

void ShowUsageWithFlags(const char *argv0) {
  fprintf(stdout, "%s: %s\n", Basename(argv0), ProgramUsage());

  vector<CommandLineFlagInfo> flags;
  GetAllFlags(&flags);           // flags are sorted by filename, then flagname

  for (vector<CommandLineFlagInfo>::const_iterator flag = flags.begin();
       flag != flags.end();
       ++flag) {
    // If the flag has been stripped, pretend that it doesn't exist.
    if (flag->description == kStrippedFlagHelp) continue;
    // Now print this flag
    fprintf(stdout, "%s", DescribeOneFlag(*flag).c_str());
  }
}

// Convert the help, program, and usage to xml.
static void ShowXMLOfFlags(const char *prog_name) {
  vector<CommandLineFlagInfo> flags;
  GetAllFlags(&flags);   // flags are sorted: by flagname

  // XML.  There is no corresponding schema yet
  fprintf(stdout, "<?xml version=\"1.0\"?>\n");
  // The document
  fprintf(stdout, "<AllFlags>\n");
  // the program name and usage
  fprintf(stdout, "<program>%s</program>\n",
          XMLText(Basename(prog_name)).c_str());
  fprintf(stdout, "<usage>%s</usage>\n",
          XMLText(ProgramUsage()).c_str());
  // All the flags
  for (vector<CommandLineFlagInfo>::const_iterator flag = flags.begin();
       flag != flags.end();
       ++flag) {
    if (flag->description != kStrippedFlagHelp)
      fprintf(stdout, "%s\n", DescribeOneFlagInXML(*flag).c_str());
  }
  // The end of the document
  fprintf(stdout, "</AllFlags>\n");
}

// --------------------------------------------------------------------
// ShowVersion()
//    Called upon --version.  Prints build-related info.
// --------------------------------------------------------------------

static void ShowVersion() {
  const char* version_string = VersionString();
  if (version_string && *version_string) {
    fprintf(stdout, "%s version %s\n",
            ProgramInvocationShortName(), version_string);
  } else {
    fprintf(stdout, "%s\n", ProgramInvocationShortName());
  }
# if !defined(NDEBUG)
  fprintf(stdout, "Debug build (NDEBUG not #defined)\n");
# endif
}

static void AppendPrognameStrings(vector<string>* substrings,
                                  const char* progname) {
  string r;
  r += PATH_SEPARATOR;
  r += progname;
  substrings->push_back(r + ".");
  substrings->push_back(r + "-main.");
  substrings->push_back(r + "_main.");
}

// --------------------------------------------------------------------
// HandleCommandLineHelpFlags()
//    Checks all the 'reporting' commandline flags to see if any
//    have been set.  If so, handles them appropriately.  Note
//    that all of them, by definition, cause the program to exit
//    if they trigger.
// --------------------------------------------------------------------

void HandleCommandLineHelpFlags() {
  const char* progname = ProgramInvocationShortName();

  vector<string> substrings;
  AppendPrognameStrings(&substrings, progname);

  if (FLAGS_help) {
    // show all options
    ShowUsageWithFlags(progname);
    gflags_exitfunc(1);

  } else if (FLAGS_help_xml) {
    ShowXMLOfFlags(progname);
    gflags_exitfunc(1);

  } else if (FLAGS_version) {
    ShowVersion();
    // Unlike help, we may be asking for version in a script, so return 0
    gflags_exitfunc(0);

  }
}


} // namespace GFLAGS_NAMESPACE
