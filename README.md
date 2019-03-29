__7-Zip-FL2__ is a fork of mainline 7-Zip which uses the [Fast LZMA2 Library] as
the default compressor.

[Fast LZMA2 Library]: https://github.com/conor42/fast-lzma2

Binary releases are avilable on the [release page].

[release page]: https://github.com/conor42/7-Zip-FL2/releases

To use the standard 7-Zip encoder, specify -m0=lzma2 on the command line.

The help file in this project has not been updated with information about Fast
LZMA2. See the document releaseFL2.txt in the releases for information.

### Status

The command-line version has been subjected to automated testing on sets of files
chosen at random. The FL2 component has passed heavy fuzz testing. Even so, no
warranty or fitness for a particular purpose is expressed or implied. The Fast
LZMA2 Library is distributed under the licenses found in COPYING and LICENSE in
the directory C/fast-lzma2.