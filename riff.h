/*
libriff

Author/copyright: Markus Wolf
License: zlib (https://opensource.org/licenses/Zlib)


To read any RIFF files.
Not specialized to specific types like AVI or WAV.
Special chunks (e.g. "LIST") can contain a nested sub list of chunks


Example structure of RIFF file:

chunk level start ("RIFF") - ID,size,type
  type (of parent chunk header)
  chunk block - level 0
  chunk block ("LIST") - level 0 - ID,size,type
    type (of parent chunk header)
    chunk block - level 1
    chunk block - level 1
  chunk block - level 0


Usage:
Use a default allocator function (file, mem) or create your own
 The required function pointers must be set for reading and seeking
 When creating your own, have a look at the default function code as template
After the file is opened we are in the first chunk at level 0
 You can freely read and seek within the data of the current junk
 Use riff_nextChunk() to go to the first data byte of the next junk (chunk header is read already then)
If the current junk contains a list of sub level chunks, call riff_subInto() to be positioned at the first data byte of the first sub level chunk
 Call riff_subBackNext() to go to the next chunk in the parent level (seek forward, not backward)
 Call riff_subBack() to go to the data start of the parent level chunk (seek backward, not forward)
Read members of the riff_handle to get 
*/


#ifndef _RIFF_H_
#define _RIFF_H_



#define RIFF_HEADER_SIZE  12      //size of RIFF file header and RIFF/LIST chunks that contain subchunks
#define RIFF_CHUNK_DATA_OFFSET 8  //offset from start of chunk, size of chunk ID + chunk size field.


//Error codes (pass to riff_errorToString()), value mapping may change in the future
//non critical
#define RIFF_ERROR_NONE   0  //no error
#define RIFF_ERROR_EOC    1  //end of current chunk, when trying to read/seek beyond end of current chunk data
#define RIFF_ERROR_EOCL   2  //end of chunk list, if you are already at the last chunk in the current list level, occures when trying to seek the next chunk
#define RIFF_ERROR_EXDAT  3  //excess data at end of file beyond level 0 chunk list, not critical, the rest is simply ignored

//critical errors
#define RIFF_ERROR_CRITICAL  4  //first critical error code (to be used for <,> condition)

#define RIFF_ERROR_ILLID     4  //illegal ID, ID (type) contains not printable or non ASCII characters
#define RIFF_ERROR_ICSIZE    5  //invalid chunk size value in chunk header, value exceeds list level or file - indicates corruption or cut off file 
#define RIFF_ERROR_EOF       6  //unexpected end of RIFF file, indicates corruption (wrong chunk size field) or a cut off file or the passed size parameter was wrong (too small) upon opening
#define RIFF_ERROR_ACCESS    7  //access error, indicating that the file is not accessible (permissions, invalid file handle, etc.)



/*
//riff header, also used for sub levels
//unpacked -> do not read directly to it from file
typedef struct riff_header
{
	unsigned char chunkID[5];    //offs 0x00 (with terminator to be printable)
	int chunkDataSize;            //offs 0x04
	unsigned char riffType[5];   //offs 0x08 
} riff_header;
*/


//level stack entry
//needed to retrace from sub level chunk
//data of parent
struct riff_levelStackE {
	size_t c_pos_start;        //absolute chunk position in file stream, start of chunk header
	unsigned char c_id[5];    //ID of chunk
	size_t c_size;             //chunk size without chunk header (value as stored in RIFF file)
	unsigned char c_type[5];  //(form) type ID of chunk (available for all chunks containing sub chunks) - at level 0 it is the RIFF form type
};


//RIFF handle structure
//- Members are public and intended for read access (to avoid a plethora of get-functions)
//  Be careful with the stack, check "ls_size" first
typedef struct riff_handle {
	//RIFF file header info, available once the file is opened (could have been put)
	char h_id[5];      //"RIFF" + terminator
	size_t h_size;     //size value given in header (h_size + 8 == file_size)
	char h_type[5];    //type of file FOURCC + terminator
	size_t pos_start;  //start pos of RIFF file

	size_t size;      //total size of RIFF file
	size_t pos;       //current position in stream
	
	size_t c_pos_start; //start pos of current chunk (absolute pos)
	size_t c_pos;       //position in current chunk (offset in data block)
	//int c_pos_data;    //position in chunk data (c_pos + 8)
	char c_id[5];       //id of current chunk + terminator
	size_t c_size;      //size of current chunk data in bytes (value stored in file), excluding chunk header
	char pad;           //1 if c_size is odd, else 0 (indicates unused extra byte at end of chunk)

	struct riff_levelStackE *ls;   //level stack, resizes dynamically, to access the parent chunk data: h->ls[h->ls_level-1]
	size_t ls_size;     //size of stack in num. elements, stack extends automatically if needed
	int ls_level;       //current level, starts at 0
	
	void *fh;  //file handle or memory address, only accessed by user FP functions
	
	
	//For internal use, don't call directly:
	
	//void (*fp_init)()    //optional
	//void (*fp_deinit)()  //optional (close file stream, etc.)
	
	size_t (*fp_read)(void *fh, void *ptr, size_t size);    //required
	size_t (*fp_seek)(void *fh, size_t pos);    //required
	
	//fp_write()
	
} riff_handle;



//*** external ***



//functions to parse a riff file
size_t riff_readInChunk(riff_handle *rh, void *to, size_t size); //read in current chunk, returns RIFF_ERROR_EOC if end of chunk is reached
int riff_seekInChunk(riff_handle *rh, size_t c_pos);      //seek in current chunk, returns RIFF_ERROR_EOC if end of chunk is reached, pos 0 is first byte after chunk size (chunk offset 8)

int riff_seekNextChunk(struct riff_handle *rh);       //seek to start of next chunk within current level, ID and size is read automatically, return
//int riff_seekNextChunkID(struct riff_handle *rh, char *id);  //find and go to next chunk with id (4 byte) in current level, fails if not found - position is invalid then -> maybe not needed, the user can do it via simple loop
int riff_seekChunkStart(struct riff_handle *rh);      //seek back to start of curren chunk
int riff_rewind(struct riff_handle *rh);              //seek back to very first chunk of file at level 0, the position just after opening via riff_open_...()
int riff_seekLevelStart(struct riff_handle *rh);      //goto start of first data byte of first chunk in current level (seek backward)

int riff_seekLevelSub(struct riff_handle *rh);        //goto sub level chunk (auto seek to start of parent chunk if not already there); "LIST" chunk typically contains a list of sub chunks
int riff_levelParent(struct riff_handle *rh);         //step back from sub level; position doesn't change and you are inside the data section of the parent level (not at the beginning of it!)

//int riff_seekLevelParent(struct riff_handle *rh);     //go back from sub level to the start of parent chunk (seek backward)
//int riff_seekLevelParentNext(struct riff_handle *rh); //go back from sub level to the start of the chunk following the parent chunk (seek forward)

//validate chunk level structure, seeks to the first byte of the current level, seeks from chunk header to chunk header
//to check all sub levels you need to define a recursive function; by comparing a chunk ID you have to decide if it contains a sub level chunk list to visit according to the RIFF type
//file position is at end of file after calling
int riff_levelValidate(struct riff_handle *rh);

//return string to error code
//the current position (h->pos) tells you where in the file the problem occured
const char *riff_errorToString(int e);

//deallocate riff_handle and contained stack, file source (memory) is not closed or freed
void riff_free(riff_handle *rh);



//*** user IO init ***



//allocation base, allocate and return handle; to call from user IO open function
riff_handle *riff_handleAllocate();

//read RIFF file header, return error code; to call from user IO functions
int riff_readHeader(riff_handle *rh);




//** default FP setup **



//create and return initialized RIFF handle, FPs are set up for file access
//file position must be at the start of RIFF file, which can be nested in another file (file pos > 0)
//pass size as 0 for unknown size, giving the correct size helps to identify file corruption
riff_handle *riff_open_file(FILE *f, size_t size);

//create and return initialized RIFF handle, FPs are set up to default for memory access
riff_handle *riff_open_mem(void *ptr, size_t size);


//user open - must handle "riff_handle" allocation and setup
// e.g. for file access via network socket
// see and use "riff_open_file()" definition as template
// "riff_handle open_user(FOO, size_t size)";




#endif // _RIFF_H_
