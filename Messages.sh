#! /bin/sh
$EXTRACTRC *.ui */*.ui > rc.cpp || exit 11
$EXTRACTRC *.rc */*.rc >> rc.cpp || exit 12
$XGETTEXT *.cpp */*.cpp *.h */*.h -o $podir/krdc.pot
