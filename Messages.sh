#! /bin/sh
$EXTRACTRC *.ui */*.ui >> rc.cpp || exit 11
$XGETTEXT *.cpp */*.cpp *.h -o $podir/krdc.pot
