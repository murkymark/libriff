// Example for usage of libriff
//
// We open a potential RIFF file, recursively traverse through all chunks and print the chunk header info with indentation.
//


#include <stdio.h>
#include <string.h>

#include "riff.h"





int nlist = 0;  //number of sub lists (LIST chunks)
int nchunk = 0; //number of chunks




void test_traverse_rec(riff_handle *rh){
	int err;
	char indent[512*8];  //indentation
	
	//identation for pretty output
	strcpy(indent, "");
	for(int i = 0; i < rh->ls_level; i++)
		sprintf(indent, "%s ", indent);
	
	if(rh->ls_level == 0) {
		printf("CHUNK_ID: TOTAL_CHUNK_SIZE [CHUNK_DATA_FROM_TO_POS]\n");
		//output RIFF file header
		printf("%s%s: %d [%d..%d]\n", indent, rh->h_id, rh->h_size, rh->pos_start, rh->pos_start + rh->size);
		printf(" %sType: %s\n", indent, rh->h_type);
	}
	else {
		//output type of parent list chunk
		struct riff_levelStackE *ls = rh->ls + rh->ls_level - 1;
		//type ID of sub list is only read, after stepping into it
		printf(" %sType: %s\n", indent, ls->c_type);
	}
	
	strcat(indent, " ");
	
	int k = 0;
	
	while(1){
		printf("%s%s: %d [%d..%d]\n", indent, rh->c_id, rh->c_size, rh->c_pos_start,  rh->c_pos_start + 8 + rh->c_size + rh->pad - 1);
		
		//if current chunk not a chunk list
		if(strcmp(rh->c_id, "LIST") != 0  &&  strcmp(rh->c_id, "RIFF") != 0){
		}
		else {
			//getchar(); //uncomment to press ENTER to continue after a printed chunk
			{
				err = riff_seekLevelSub(rh);
				if (err){
				}
				else
					nlist++;
				test_traverse_rec(rh); //recursive call
			}
		}
		k++;
		
		err = riff_seekNextChunk(rh);
		if(err >= RIFF_ERROR_CRITICAL) {
			printf("%s", riff_errorToString(err));
			break;
		}
		else if (err < RIFF_ERROR_CRITICAL  &&  err != RIFF_ERROR_NONE) {
			//printf("last chunk in level %d %d .. %d %s\n", rh->ls_level, rh->c_pos_start, rh->c_pos_start + 8 + rh->c_size + rh->pad, rh->c_id);
			
			//go back from sub level
			riff_levelParent(rh);
			//file pos has not changed by going a level back, we are now within that parent's data
			break;
		}
		else
			nchunk++;
	}
}




void test(FILE *f){
	//get size
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	//allocate initialized handle struct
	riff_handle *rh = riff_handleAllocate();
	
	//after allocation rh->fp_fprintf == fprintf
	//you can change the rh->fp_fprintf function pointer here for error output
	//rh->fp_fprint = NULL;  //set to NULL to disable any error printing
	
	//open file, use build in input wrappers for file
	//open RIFF file via file handle -> reads RIFF header and first chunk header
	if(riff_open_file(rh, f, fsize) != RIFF_ERROR_NONE){
		return;
	}
	nchunk++; //header can be seen as chunk
	
	test_traverse_rec(rh);
	printf("\nlist chunks: %d\nchunks: %d\n", nlist, nchunk);
	
	int r;
	
	
	//current list level
	printf("\n");
	printf("Current pos: %d\n", rh->pos);
	printf("Current list level: %d\n", rh->ls_level);
	
	
	//read a byte
	printf("Reading a byte of chunk data ...\n");
	char buf[1];
	r = riff_readInChunk(rh, buf, 1);
	printf("Bytes read: %d of %d\n", r, 1);
	printf("Current pos: %d\n", rh->pos);
	printf("Current list level: %d\n", rh->ls_level);
	
	
	printf("seeking a byte forward in chunk data ...\n");
	r = riff_seekInChunk(rh, rh->c_pos + 1);
	if(r != RIFF_ERROR_NONE)
		printf("Seek failed!\n");
	printf("Current pos: %d\n", rh->pos);
	printf("Current list level: %d\n", rh->ls_level);
	
	
	//rewind to first chunk's data pos 0
	printf("Rewind to first chunk in file ...\n");
		r = riff_rewind(rh);
	if(r != RIFF_ERROR_NONE)
		printf("Error: %s\n", riff_errorToString(r));
	printf("Current pos: %d (expected: %d)\n", rh->pos, rh->pos_start + RIFF_HEADER_SIZE + RIFF_CHUNK_DATA_OFFSET);
	printf("Current list level: %d\n", rh->ls_level);
	
	
	riff_handleFree(rh);
	
	//find and visit all LIST chunks
	
	//load file to mem and do same again
}




int main(int argc, char *argv[] ){
	if(argc < 2){
		printf("Need path to input RIFF file!\n");
		return -1;
	}
	
	printf("Opening file: %s\n", argv[1]);
	FILE *f = fopen(argv[1],"rb");
	
	if(f == NULL) {
		printf("Failed to open file!\n");
		return -1;
	}
	
	test(f);
	
	fclose(f);
	
	return 0;
}
