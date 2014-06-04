Talkcoin integration/staging tree
================================

http://www.talkcoin.org

Copyright (c) 2009-2013 Bitcoin Developers
Copyright (c) 2014 Talkcoin Developers

What is Talkcoin?
----------------

Talkcoin is the first cryptocurrency to offer a true decentralized chat program that won't rely on a central server to deliver messages from the sender to their recipients and lets you communicate freely with anyone in the world without censorship.
It also has a decentralized block reward voting, fast transactions, new efficient algorithm / GPU miner (based on NIST5: BLAKE - Grøstl - JH - Keccak - Skein).

For more information, as well as an immediately useable, binary version of
the Talkcoin client sofware, see http://www.talkcoin.org.

License
-------

Talkcoin is released under the terms of the MIT license. See `COPYING` for more
information or see http://opensource.org/licenses/MIT.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the Talkcoin
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion (if they haven't already) on the
[mailing list](http://sourceforge.net/mailarchive/forum.php?forum_name=bitcoin-development).

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see `doc/coding.txt`) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/bitcoin/bitcoin/tags) are created
regularly to indicate new official, stable release versions of Talkcoin.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test. Please be patient and help out, and
remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write unit tests for new code, and to
submit new unit tests for old code.

Unit tests for the core code are in `src/test/`. To compile and run them:

    cd src; make -f makefile.unix test

Unit tests for the GUI code are in `src/qt/test/`. To compile and run them:

    qmake TALKCOIN_QT_TEST=1 -o Makefile.test talkcoin-qt.pro
    make -f Makefile.test
    ./talkcoin-qt_test
