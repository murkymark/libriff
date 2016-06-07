#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "riff.h"


#define RIFF_LEVEL_ALLOC 16  //number of elements allocated more when needing to enlarge (step)


//table to translate Error code to string
const char *riff_es[] = {
	//0
	"No error",
	//1
	"End of chunk",
	//2
	"End of chunk list",
	//3
	"Excess bytes at end of file",
	
	//4
	"Illegal four character id",
	//5
	"Chunk size exceeds list level or file",
	//6
	"End of RIFF file",
	//7
	"File access failed"
	
	//8
	"Unknown RIFF ERROR"  //all other
};


//*** default access FP setup ***


//** FILE **


size_t read_file(void *fh, void *ptr, size_t size){
	return fread(ptr, 1, size, (FILE*)fh);
}

size_t seek_file(void *fh, size_t pos){
	fseek((FILE*)fh, pos, SEEK_SET);
	return pos;
}

//description: see header file
riff_handle *riff_open_file(FILE *f, size_t size){
	riff_handle *rh = riff_handleAllocate();
	if(rh != NULL){
		rh->fh = f;
		rh->size = size;
		rh->pos_start = ftell(f); //file offset of start of RIFF stream
		
		rh->fp_read = &read_file;
		rh->fp_seek = &seek_file;
		
		riff_readHeader(rh);
	}
	return rh;
}


//** memory **


size_t read_mem(void *fh, void *ptr, size_t size){
	memcpy(ptr, fh, size);
	return size;
}

size_t seek_mem(void *fh, size_t pos){
	return pos; //instant in memory
}

//description: see header file
riff_handle *riff_open_mem(void *ptr, size_t size){
	riff_handle *rh = riff_handleAllocate();
	if(rh != NULL){
		rh->fh = ptr;
		rh->size = size;
		//rh->pos_start = 0 //redundant
		
		rh->fp_read = &read_mem;
		rh->fp_seek = &seek_mem;
		
		riff_readHeader(rh);
	}
	return rh;
}


//*** internal ***


//pass pointer to 32 bit LE value and convert, return in native byte order
unsigned int convUInt32LE(void *p){
	unsigned char *c = (unsigned char*)p;
	return c[0] | (c[1] << 8) | (c[2] << 16) | (c[3] << 24);
}


//read 32 bit LE from file via FP and return as native
unsigned int readUInt32LE(riff_handle *rh){
	char buf[4] = {0};
	rh->fp_read(rh->fh, buf, 4);
	rh->pos += 4;
	rh->c_pos += 4;
	return convUInt32LE(buf);
}


//read chunk header
int riff_readChunkHeader(riff_handle *rh){
	char buf[8];
	
	int n = rh->fp_read(rh->fh, buf, 8);
	
	if(n != 8){
		//printf("failed to read header, %d of %d bytes read\n", n, 8);
		return -1; //return error code
	}
	
	rh->c_pos_start = rh->pos;
	rh->pos += n;
	
	memcpy(rh->c_id, buf, 4);
	rh->c_size = convUInt32LE(buf + 4);
	rh->pad = rh->c_size & 0x1;
	rh->c_pos = 0;

	//verify valid chunk ID, must contain only printable ASCII chars
	int i;
	for(i = 0; i < 4; i++) {
		if(rh->c_id[i] < 0x20  ||  rh->c_id[i] > 0x7e) {
			fprintf(stderr,"Invalid chunk ID (FOURCC) of chunk at file pos %d: 0x%02x,0x%02x,0x%02x,0x%02x\n", rh->c_pos_start, rh->c_id[0], rh->c_id[1], rh->c_id[2], rh->c_id[3]);
			return -1;
		}
	}
	
	return 0;
}


//pop from level stack
//when returning we are positioned inside the parent chunk ()
void stack_pop(riff_handle *rh){
	if(rh->ls_level <= 0)
		return;
	
	rh->ls_level--;
	struct riff_levelStackE *ls = rh->ls + rh->ls_level;
	
	rh->c_pos_start = ls->c_pos_start;
	memcpy(rh->c_id, ls->c_id, 4);
	rh->c_size = ls->c_size;
	rh->pad = rh->c_size & 0x1; //pad if chunk sizesize is odd
	
	rh->c_pos = rh->pos - rh->c_pos_start - RIFF_CHUNK_DATA_OFFSET;
}


//push to level stack
void stack_push(riff_handle *rh, char *type){
	//need to enlarge stack?
	if(rh->ls_size < rh->ls_level + 1){
		size_t ls_size_new = rh->ls_size * 2; //double size
		if(ls_size_new == 0)
			ls_size_new = RIFF_LEVEL_ALLOC; //default stack allocation
		
		struct riff_levelStackE *lsnew = malloc(ls_size_new * sizeof(struct riff_levelStackE));
		rh->ls_size = ls_size_new;
		
		//need to copy?
		if(rh->ls_level > 0){
			memcpy(lsnew, rh->ls, rh->ls_level * sizeof(struct riff_levelStackE));
		}
		
		//free old
		if(rh->ls != NULL)
			free(rh->ls);
		rh->ls = lsnew;
	}
	
	struct riff_levelStackE *ls = rh->ls + rh->ls_level;
	ls->c_pos_start = rh->c_pos_start;
	strcpy(ls->c_id, rh->c_id);
	ls->c_size = rh->c_size;
	//printf("list size %d\n", (rh->ls[rh->ls_level].size));
	strcpy(ls->c_type, type);
	rh->ls_level++;
}


//*** user access ***


//description: see header file
riff_handle *riff_handleAllocate(){
	riff_handle *rh = calloc(1, sizeof(riff_handle));
	if(rh != NULL){
	}
	return rh;
}


//description: see header file
int riff_readHeader(riff_handle *rh){
	char buf[RIFF_HEADER_SIZE];
	
	if(rh->fp_read == NULL) {
		fprintf(stderr, "IO function pointer not set\n"); //fatal user error
		return -1;
	}
	
	int n = rh->fp_read(rh->fh, buf, RIFF_HEADER_SIZE);
	rh->pos += n;
	
	if(n != RIFF_HEADER_SIZE){
		printf("read error, failed to read header\n");
		//printf("%d", n);
		return -1; //return error code
	}
	memcpy(rh->h_id, buf, 4);
	rh->h_size = convUInt32LE(buf + 4);
	memcpy(rh->h_type, buf + 8, 4);


	if(strcmp(rh->h_id, "RIFF") != 0) {
		printf("invalid RIFF header\n");
		return -1;
	}
	
	riff_readChunkHeader(rh);
	
	//compare with given file size
	if(rh->size != 0){
		if(rh->size != rh->h_size + RIFF_CHUNK_DATA_OFFSET){
			printf("size mismatch");
			return -1; //just a warning, file can still be used
		}
	}

	return 0;
}



//*** external ***


//make use of user defined functions via FPs


//read to memory block, returns number of successfully read bytes
//keep track of position, do not read beyond end of chunk, pad byte is not read
size_t riff_readInChunk(riff_handle *rh, void *to, size_t size){
	size_t left = rh->c_size - rh->c_pos;
	if(left < size)
		size = left;
	size_t n = rh->fp_read(rh->fh, to, size);
	rh->pos += n;
	rh->c_pos += n;
	return n;
}


//seek byte position in current chunk data from start of chunk data, return error on failure
//keep track of position
int riff_seekInChunk(riff_handle *rh, size_t c_pos){
	if(c_pos < 0  ||  c_pos > rh->c_size){
		return -1;
	}
	rh->pos = rh->c_pos_start + RIFF_CHUNK_DATA_OFFSET + c_pos;
	int ret = rh->fp_seek(rh->fh, rh->pos);
	rh->c_pos = c_pos;
	return ret;
}


//description: see header file
void riff_free(riff_handle *rh){
	if(rh == NULL)
		return;
	//free stack
	if(rh->ls != NULL)
		free(rh->ls);
	free(rh);
}


//description: see header file
int riff_seekNextChunk(riff_handle *rh){
	size_t posnew = rh->c_pos_start + RIFF_CHUNK_DATA_OFFSET + rh->c_size + rh->pad;
	
	size_t posmax;
	if(rh->ls_level > 0){
		struct riff_levelStackE *ls = rh->ls + rh->ls_level - 1;
		posmax = ls->c_pos_start + RIFF_CHUNK_DATA_OFFSET + ls->c_size; //max pos without possible pad byte
	}
	else
		posmax = rh->pos_start + RIFF_CHUNK_DATA_OFFSET + rh->h_size; //at level 0
	
	//printf("maxpos %d  posnew %d\n", posmax, posnew);
	
	//if no more chunks in the current sub level
	if(posmax < posnew + RIFF_CHUNK_DATA_OFFSET){
		return -1;
	}
	
	rh->fp_seek(rh->fh, posnew);
	rh->pos = posnew;
	
	return riff_readChunkHeader(rh);
}


//description: see header file
int riff_seekLevelSub(riff_handle *rh){
	//according to "https://en.wikipedia.org/wiki/Resource_Interchange_File_Format" only RIFF and LIST chunk IDs can contain subchunks
	if(strcmp(rh->c_id, "LIST") != 0  &&  strcmp(rh->c_id, "RIFF") != 0){
		fprintf(stderr, "%s() failed for ID \"%s\", only RIFF or LIST chunk can contain subchunks", __func__, rh->c_id);
		return -1;
	}
	
	//check size of parent chunk data, must be at least 4 for type ID (is empty list allowed?)
	if(rh->c_size < 4){
		printf("chunk too small to contain sub level chunks\n");
		return -1;
	}
	
	//seek to chunk start if needed
	if(rh->c_pos > 0) {
		rh->fp_seek(rh->fh, rh->c_pos_start + RIFF_CHUNK_DATA_OFFSET);
		rh->pos = rh->c_pos_start + RIFF_CHUNK_DATA_OFFSET;
		rh->c_pos = 0;
	}
	//read type ID
	unsigned char type[5] = "\0\0\0\0\0";
	rh->fp_read(rh->fh, type, 4);
	rh->pos += 4;
	//verify type ID
	int i;
	for(i = 0; i < 4; i++) {
		if(type[i] < 0x20  ||  type[i] > 0x7e) {
			fprintf(stderr,"Invalid chunk type ID (FOURCC) of chunk at file pos %d: 0x%02x,0x%02x,0x%02x,0x%02x\n", rh->c_pos_start, type[0], type[1], type[2], type[3]);
			return -1;
		}
	}
	
	//add parent chunk data to stack
	//push
	stack_push(rh, type);
	
	return riff_readChunkHeader(rh);
}


//description: see header file
int riff_levelParent(struct riff_handle *rh){
	if(rh->ls_level <= 0)
		return -1;
	stack_pop(rh);
	return 0;
}


//description: see header file
const char *riff_errorToString(int e){
	//map error to error string
	//Make sure mapping is correct!
	switch (e){
		case RIFF_ERROR_NONE:
			return riff_es[0];
			break;
		case RIFF_ERROR_EOC:
			return riff_es[1];
			break;
		case RIFF_ERROR_EOCL:
			return riff_es[2];
			break;
		case RIFF_ERROR_EXDAT:
			return riff_es[3];
			break;
		
		case RIFF_ERROR_ILLID:
			return riff_es[4];
			break;
		case RIFF_ERROR_ICSIZE:
			return riff_es[5];
			break;
		case RIFF_ERROR_EOF:
			return riff_es[6];
			break;
		case RIFF_ERROR_ACCESS:
			return riff_es[7];
			break;
		
		default:
			return  riff_es[8];
			break;
	}
}

//validate all, follow LIST chunks
//check for duplicate chunk id in one evel


//validate level
