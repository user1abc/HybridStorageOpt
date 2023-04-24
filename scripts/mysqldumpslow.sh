#!@PERL_PATH@

<<<<<<< HEAD
# Copyright (c) 2000, 2022, Oracle and/or its affiliates.
# Copyright (c) 2016, Percona Inc.
||||||| 863f74007be
# Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
# Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> mysql/5.7
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA

# mysqldumpslow - parse and summarize the MySQL slow query log

# Original version by Tim Bunce, sometime in 2000.
# Further changes by Tim Bunce, 8th March 2001.
# Handling of strings with \ and double '' by Monty 11 Aug 2001.

usage();

sub usage {
    my $text= <<HERE;
"mysqldumpslow.sh" is not currently compatible with Percona extended slow query
log format. Please use "pt-query-digest" from Percona Toolkit instead
(https://www.percona.com/doc/percona-toolkit/2.2/pt-query-digest.html).

HERE
    print $text;
    exit 0;
}
