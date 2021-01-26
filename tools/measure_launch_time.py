#!/usr/bin/env python3
#
# This script measures launching time
# Copyright (c) Jolla Ltd. 2021
#
# You may use this file under the terms of the BSD license as follows:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer
#      in the documentation and/or other materials provided with the
#      distribution.
#   3. Neither the names of the copyright holders nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from os import putenv
from sys import stdin, stdout, stderr
import argparse
import dataclasses
import datetime
import itertools
import re
import time
import subprocess

ENVVARS = {
    'QT_MESSAGE_PATTERN': "[%{category}] %{message}",
}

RULES = [
    "default.debug=true",
    "qt.scenegraph.time.renderloop.debug=true",
]

MATCHER = re.compile(r"""^
    # Journal timestamp in short-iso-precise format
    (?:(?P<journal>\d+-\d+-\d+T\d+:\d+:\d+\.\d+)(?:\+\d+)?\s\w+\s[^:]+:\s)?
    # Identifier (category or such), if missing full line parsers are used
    (?:\[(?P<identifier>[^;\]]+)\]\s+)?
    # Reason, anything except ; characters, until the end of the line
    (?P<reason>[^;]+)$""",
    re.VERBOSE
)

@dataclasses.dataclass
class Measured:
    name: str
    reason: str
    since_start: float
    since_last: float

    start = None
    last = None

    def __init__(self, name, reason, timestamp):
        self.name = name
        self.reason = reason
        if type(timestamp) == str:
            timestamp = self._parse_journal_timestamp(timestamp)
        if Measured.start is None:
            Measured.start = timestamp
            Measured.last = timestamp
        self.since_start = timestamp - Measured.start
        self.since_last = timestamp - Measured.last
        Measured.last = timestamp

    def __repr__(self):
        return (f"{self.name}; {self.reason}; "
                f"{self.since_start:.3f}; {self.since_last:+.3f}")

    def __bool__(self):
        return True

    @staticmethod
    def _parse_journal_timestamp(time_string):
        return datetime.datetime.fromisoformat(time_string).timestamp()

class LogParser:
    """Returns timestamps for logging category lines with given name.

    You can add new parsers for different logging categories by adding new
    lines to parsers dictionary below. Use logging category as the key to match
    and give LogParser a name that you want to see in the output. By default
    identifier (e.g. logging category for Qt logging) is used as name.

    To match multiple identifiers with the same LogParser create one parser and
    add it to parsers dictionary multiple times.
    """
    def __init__(self, name=None):
        self.name = name

    def get_name(self, match):
        return match.group("identifier") if self.name is None else self.name

    def get_reason(self, match):
        return match.group("reason")

    @staticmethod
    def pick_timestamp(match, timestamp):
        return match.group("journal") if timestamp is None else timestamp

    def new(self, match, timestamp):
        name = self.get_name(match)
        reason = self.get_reason(match)
        timestamp = self.pick_timestamp(match, timestamp)
        return Measured(name, reason, timestamp)

class FilteredLogParser(LogParser):
    """Like LogParser but uses regular expression to match only some of the
    output lines.

    Matches only the very first occurence.

    These can be used for full_line_parsers but please use them sparingly
    because they are matched for every line that doesn't have an identifier.
    In that case, define name.
    """
    def __init__(self, regex, *args, **kwargs):
        self.regex = re.compile(regex)
        self.captured = set()
        super().__init__(*args, **kwargs)

    @staticmethod
    def _get_reason_string(match):
        return match.group(0) if match.lastindex is None else match.group(1)

    def get_reason(self, match):
        captured = self.regex.match(match.group("reason"))
        return self._get_reason_string(captured)

    def new(self, match, *args, **kwargs):
        reason_match = self.regex.match(match.group("reason"))
        if reason_match:
            reason = self._get_reason_string(reason_match)
            if reason not in self.captured:
                self.captured.add(reason)
                return super().new(match, *args, **kwargs)

parsers = {
    "general": LogParser(),
    "sailjail": FilteredLogParser(r"^(\/usr\/bin\/firejail) .*$"),
    "default": FilteredLogParser(r"^Using Wayland-EGL$", "app"),
    "qt.scenegraph.time.renderloop": FilteredLogParser(
        r"^(Frame prepared) .*QQuickView.*$", "app")
}

full_line_parsers = [
    # Firejail doesn't use prefixes on its logging
    FilteredLogParser(r"^(Child process initialized) in .*$", "firejail"),
]

def print_title(output=stdout):
    print("timer; reason; time; difference", file=output)

title_printed = False

def parse(line, output=stdout, timestamp=None):
    match = MATCHER.match(line)
    if match:
        measured = None
        if match.group("identifier") is None:
            for parser in full_line_parsers:
                measured = parser.new(match, timestamp)
                if measured:
                    break
        elif match.group("identifier") in parsers:
            measured = parsers[match.group("identifier")].new(match, timestamp)
        if measured:
            global title_printed
            if not title_printed:
                print_title(output)
                title_printed = True
            print(measured, file=output, flush=True)
        return bool(measured)
    return False

def get_environment(with_logging_rules=True, with_logging_to_console=True):
    envvars = ENVVARS.copy()
    if with_logging_rules:
        envvars["QT_LOGGING_RULES"] = ";".join(RULES)
    if with_logging_to_console:
        envvars["QT_LOGGING_TO_CONSOLE"] = "1"
    return envvars

SAILJAIL_BINARIES = ("sailjail", "/usr/bin/sailjail")

def append_sailjail_arguments(args):
    """Append arguments for sailjail to enable sufficient logging"""
    sailjail_arg = None
    for i, arg in enumerate(args):
        if arg.endswith("sailjail") and arg in SAILJAIL_BINARIES:
            sailjail_arg = i
            break
    else:
        return  # No sailjail, don't do anything
    for arg in itertools.islice(args, i, None):
        if arg.startswith("-v") or arg == "--verbose":
            # FIXME: This doesn't handle application arguments well
            break  # Verbose is already there, nothing to do
    else:
        args.insert(i+1, "-v")
    args.insert(i+1, "--output=stderr")

def run(args):
    for var, value in get_environment().items():
        putenv(var, value)
    append_sailjail_arguments(args.args)
    if not args.without_start_time:
        parse("[general] starting process", args.output, time.time())
    p = subprocess.Popen(args.args, stderr=subprocess.PIPE)
    while p.poll() is None:
        line = p.stderr.readline()
        ts = time.time()
        if not parse(line.rstrip().decode('utf-8', 'ignore'), args.output, ts):
            if args.output == stderr:
                stdout.buffer.write(line)
                stdout.buffer.flush()
            else:
                stderr.buffer.write(line)
                stderr.buffer.flush()
    if args.with_end_time:
        parse("[general] ended process", args.output, time.time())

def print_env(args):
    for var, value in get_environment(args.with_logging_rules,
                                      args.with_logging_to_console).items():
        print(f"{var}=\"{value}\"")

def print_rules(args):
    if args.with_title:
        print("[Rules]")
    for rule in RULES:
        print(rule)

def parse_file(args):
    for line in args.file:
        parse(line, args.output)

def main():
    parser = argparse.ArgumentParser(description="Measure launching time",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""Typical usage:
Launch the application directly through this script:
$ %(prog)s run -- my_application [arguments ...]

or with sandboxing:
$ %(prog)s run -- sailjail -p my_profile /usr/bin/my_application [arguments ...]

or with invoker (see also note below):
$ %(prog)s run -- invoker --type=qt5 my_application [arguments ...]

Or with a combination of invoker and sailjail.
You can also launch the app some other way so that journal captures the
application output. In that case setup journal reading with 'parse' subcommand
before launching the app (as root):
# journalctl -f -o short-iso-precise --since now | %(prog)s parse

In that case you must also manually setup environment variables printed by
'env' subcommand and Qt logging rules printed by 'rules' subcommand to get the
correct logging output. Also note that there may not be a comparable starting
point message in journal. If your logging is setup correctly but you don't get
any output, check that you are using the right format option for journalctl as
only short-iso-precise is supported.

Also you may want to direct application output (stdout) to /dev/null unless you
specify a file to write to.

Note that with some types of invoker (silica-qt5 and similar) initialise their
logging beforehand and thus won't output the needed logging by default. In that
case it can be fixed by building a special version of mapplauncherd-qt5 which
reinitialises logging when application launches.

Known bugs: If you run sailjail with this script and give application argument
that can be mixed with sailjail's -v (--verbose), then you must specify -v also
for sailjail to measure time correctly.
""")
    subparsers = parser.add_subparsers(dest='subcommand', required=True)
    run_parser = subparsers.add_parser('run',
        usage="%(prog)s [--with-end-time] [--without-start-time] "
              "[--output|-o FILE] -- PROGRAM ...",
        help="Run command and measure launching time",
        epilog="""If no output file is specified, results are output to stderr
            and any non-parsed application output is diverted to stdout.""")
    run_parser.add_argument('args', nargs='*', metavar="PROGRAM ...",
        help="Program to run, with or without arguments")
    run_parser.add_argument('--with-end-time', action='store_true',
        default=False, help="Log process ending time")
    run_parser.add_argument('--without-start-time', action='store_true',
        default=False, help="Do not log process starting time")
    run_parser.add_argument('--output', '-o', metavar="FILE",
        type=argparse.FileType('w'), default=stderr,
        help="File to write. Overwritten. Writes to stderr by default")
    run_parser.set_defaults(func=run)
    env_parser = subparsers.add_parser('env',
        help="Print environment variables for manual measurement")
    env_parser.add_argument('--with-logging-rules', action='store_true',
        default=False, help="Include QT_LOGGING_RULES variable")
    env_parser.add_argument('--with-logging-to-console', action='store_true',
        default=False, help="""Include QT_LOGGING_TO_CONSOLE variable.
            Which controls whether logging is forced to console.""")
    env_parser.set_defaults(func=print_env)
    rules_parser = subparsers.add_parser('rules',
        help="Print Qt logging rules for manual measurement")
    rules_parser.add_argument('--with-title', action='store_true',
        default=False,
        help="Output [Rules] line, useful when creating the file")
    rules_parser.set_defaults(func=print_rules)
    parse_parser = subparsers.add_parser('parse',
        help="Parse journal logs from file or stdin",
        epilog="""Use the environment variables from 'env' subcommand and
            '-o short-iso-precise' argument for journalctl.
            Note that parsing journalctl output does not work on
            timestamps crossing midnight or around leap seconds
            or daylight saving time adjustments""")
    parse_parser.add_argument('file', nargs='?', metavar="FILE",
        type=argparse.FileType(), default=stdin,
        help="File to read. Reads stdin if omitted")
    parse_parser.add_argument('--without-journal-timestamps',
        action='store_true', default=False,
        help="""Don't parse and print journal timestamps even if they exist
        in file.""")
    parse_parser.add_argument('--output', '-o', metavar="FILE",
        type=argparse.FileType('w'), default=stdout,
        help="File to write. Overwritten. Writes to stdout by default")
    parse_parser.set_defaults(func=parse_file)
    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
