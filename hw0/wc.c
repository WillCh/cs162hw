#include <stdio.h>

int main (int argc, char* argv[]) {
    // 
    int lineCount = 0;
    int charCount = 0;
    int wordCount = 0;
    if (argc <= 1) {
		char ch;
		char prevCh = ' ';
		while ((ch = getchar()) != EOF) {
			charCount++;
            if (prevCh == ' ' && ch != ' ') {
				wordCount++;
            }
			// handle the line change
			if (ch == '\n' || ch == '\r') {
				lineCount++;
			}
			prevCh = ch;
		}
		// output the results
		printf("%d %d %d\n", lineCount, wordCount, charCount);
        return 0;
    }    
	char *fileName;
	if (argc == 2) {    
		fileName = argv[1];
    } else if (argc > 2) {
		fileName = argv[2];
	}
	FILE *inputFilePointer = fopen(fileName, "r");
    if (inputFilePointer == NULL) {
        fprintf(stderr, "Can't open input file!\n");
    }
    char ch;
    char prevCh = ' ';
    while ((ch = fgetc(inputFilePointer)) != EOF) {
        charCount++;
        if (prevCh == ' ' && ch != ' ') {
            wordCount++;
        }
        // handle the line change
        if (ch == '\n' || ch == '\r') {
            lineCount++;     
        }
        prevCh = ch;
    }
    // close the file
    fclose(inputFilePointer);
    // output the results
    printf("%d %d %d %s\n", lineCount, wordCount, charCount, fileName); 
    return 0;
}
