__7-Zip-FL2__ is a fork of mainline 7-Zip which uses the [Fast LZMA2 Library] as the
default compressor.

[Fast LZMA2 Library]: https://github.com/conor42/fast-lzma2

### Additional Matchfinder

7-Zip-FL2 adds a new matchfinder option, -mmf=RMF, provided by the Fast LZMA2 library.
RMF is the default for LZMA2 in this version. To use the other matchfinders in the
console version, specify -mmf=HC4 (level 1 - 4) or -mmf=BT4 (level 5+) on the command
line.

The help file in this release has not been updated with information about RMF.

### Status

The command-line version has been subjected to automated testing on sets of files
chosen at random. The FL2 component has passed heavy fuzz testing. Even so, this
software has no warranty and both 7-Zip 18.01 and FL2 0.9.0 are beta versions.