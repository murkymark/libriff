# libriff
A C (C99) library for parsing RIFF (Resource Interchange File Format) files.  

<pre>
- https://en.wikipedia.org/wiki/Resource_Interchange_File_Format
- Some common RIFF file types: WAV, AVI, ANI, MIDI SMF, MIDI DLS

- Helps to stroll around in the chunk tree structure
  provides functions like: openFile(), enterList(), leaveList(), nextChunk()
- Not specialized in or limited to any specific RIFF form type
- Supports input wrappers for file access via function pointers; wrappers for file or memory input already present
- Can be seen as simple example for a file format library supporting user defined input wrappers

See "riff.h" for further info.

TODO:
Make inline documentation Doxygen compatible
</pre>
