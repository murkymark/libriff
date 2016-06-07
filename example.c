// Example for usage of Libriff
//
//
// Example for libriff
// We open a file and recursively traverse through all chunks
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
	for(int i = 0; i < rh->ls_level + 1; i++)
		sprintf(indent, "%s ", indent);
	
	
	if(rh->ls_level == 0) {
		//output RIFF file header
		printf("%s%s %d [%d..%d]\n", indent, rh->h_id, rh->h_size, rh->pos_start + rh->size);
		printf("%sType: %s\n", indent, rh->h_type);
	}
	else {
		//output type of parent list chunk
		struct riff_levelStackE *ls = rh->ls + rh->ls_level - 1;
		//type ID of sub list is only read, after stepping into it
		printf("%sType: %s\n", indent, ls->c_type);
	}
	
	strcat(indent, " ");
	
	int k = 0;
	while(1){
	
		printf("%s%s %d [%d..%d]\n", indent, rh->c_id, rh->c_size, rh->c_pos_start,  rh->c_pos_start + 8 + rh->c_size + rh->pad - 1);

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
	
	//open file, use build in input wrappers for file
	riff_handle *rh = riff_open_file(f, fsize); //open RIFF file via file handle and read RIFF header and first chunk header
	nchunk++;
	
	test_traverse_rec(rh);
	printf("\nsub lists: %d\nchunks: %d\n", nlist, nchunk);
	
	riff_free(rh);
	
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
