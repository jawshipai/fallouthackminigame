/*
Fallout Hack Mini Game Recreation by joshua bradley
NOTE: right now the code is a mess but it works...
BUG: you can create more symbol duds by removing dud words,
and symbol duds can be found across lines which isn't a 
thing in the actual fallout game but whatever
credit for words.txt: https://gist.github.com/shmookey/b28e342e1b1756c4700f42f17102c2ff
credit for words7.txt: https://github.com/powerlanguage/word-lists/blob/master/common-7-letter-words.txt
(i modified words7.txt tolowercase)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>

#define TEST 1
#define BOARD_ROWS 16 // 16-default
#define BOARD_COLUMN_SIZE 12 // 12-default
#define BOARD_SIZE BOARD_ROWS*BOARD_COLUMN_SIZE*2
#define WORDS_FILE "words.txt"
#define WORDS_SIZE 3103 // words.txt-3103/words7.txt-1371
#define BIGGEST_WORD_LENGTH 5
#define HISTORY_ROWS BOARD_ROWS-1
#define HISTORY_LENGTH 50

char bgsymbols[] = {
    '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', 
    '.', '/', ':', ';', '<', '=', '>', '?', '@', '[', '\\', ']', '^', 
    '_', '`', '{', '|', '}', '~'
};

char board[BOARD_SIZE + 1] = {0};
int tries = 4;
char history[HISTORY_ROWS][HISTORY_LENGTH] = {0};

int totalWords = 0;
int passwordIndex = -1;
char password[BIGGEST_WORD_LENGTH] = {0};
int wordIndexes[50] = {0};
int w = 0;
int usedSymbol[50] = {0};
int u = 0;

int fakeaddr = 0;

regex_t regex_parens, regex_braces, regex_brackets, regex_angles;
int reti;

// generates a random number, [min,max] both inclusive
int randomNum(int min, int max) {
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

// update the command history
void updateHistory(char* text){
    // move everything else up
    for(int i = 1; i < HISTORY_ROWS; i++){
        for(int j = 0; j < HISTORY_LENGTH; j++){
            history[i-1][j] = history[i][j];
            if(history[i][j] == '\0') {
                j = HISTORY_LENGTH;
            }
        }
    }
    // insert new history text
    int i = 0;
    while(text[i] != '\0' && i < HISTORY_LENGTH) {
        history[HISTORY_ROWS-1][i] = text[i];
        i++;
    }
    history[HISTORY_ROWS-1][i] = '\0';
}

// prints out the board
void printBoard(){
    printf("Welcome to ROBCO Industries [TM] Termlink\n");
    printf("Password Required\n");
    printf("Attempts Remaining: ");
    for(int i = 0; i < tries; i++) {
        //printf("%c ",219);
        printf("\u25AE ");
    }
    printf("\n");
    int a = 0;
    int b = BOARD_SIZE/2;
    int h = 0;
    while(b<BOARD_SIZE) {
        printf("0x%.4X ",fakeaddr+a);
        int n = a;
        for(int i = n; i < n+BOARD_COLUMN_SIZE; i++){
            printf("%c",board[a]);
            a++;
        }
        printf(" 0x%.4X ",fakeaddr+b);
        n = b;
        for(int i = n; i < n+BOARD_COLUMN_SIZE; i++) {
            printf("%c",board[b]);
            b++;
        }
        printf(" ");
        if(h < HISTORY_ROWS) {
            printf("%s",history[h]);
            h++;
            printf("\n");
        }
        
    }
    
}

// fills the board with random characters
void fillBoardRandom() {
    for(int i = 0; i < BOARD_SIZE; i++) {
        board[i] = bgsymbols[randomNum(0, sizeof(bgsymbols)-1)];
    }
    board[BOARD_SIZE]= '\0';
}

char* getRandomWord(){
    FILE* wordFile = fopen(WORDS_FILE, "r");
    static char buffer[10] = {0};
    if(!wordFile){
        perror("no words file");
        exit(EXIT_FAILURE);
    }
    char ch;
    int desiredWordIndex = randomNum(0,WORDS_SIZE-1);
    int i = 0, b = 0, wordFound = 0;
    while((ch = fgetc(wordFile)) != EOF && !wordFound){
        if(ch == '\n') {
            i++;
        }
        if(i==desiredWordIndex){
            while((ch = fgetc(wordFile)) != EOF && ch != '\n'){
                buffer[b] = ch;
                b++;
            }
            wordFound = 1;
        }
    }
    fclose(wordFile);
    return buffer;
}

int fillBoardWords() {
    //alternate between blank space and word, checking if we have room
    int i = 0;
    int words = 0;
    while(i < BOARD_SIZE) {
        i += randomNum(10,20); // [10,20]
        if(i > BOARD_SIZE - BIGGEST_WORD_LENGTH) {
            totalWords = words;
            return words;
        }
        char* buffer = getRandomWord();
        int j = 0;
        wordIndexes[w++] = i;
        while(buffer[j] != 0) {
            board[i] = buffer[j];
            i++, j++;
        }
        words++;
    }
    board[BOARD_SIZE] = '\0';
    totalWords = words;
    return words;
}

int checkWord(int index){
    // check that we landed at an Aa-Zz (65->90 and 97->122)
    if(isalpha(board[index]) == 0) {
        return -1;
    }
    // go to left-most Aa-Zz
    int i = index;
    while(i-1 >= 0 && isalpha(board[i-1])){
        i--;
    }
    // return the index
    return i;
}

int checkSymbol(int index) {
    if(isalpha(board[index]) != 0) {
        return -1;
    }
    // Compile separate regex for each rule
    reti = regcomp(&regex_parens, "^\\([^A-Za-z(]*\\)", REG_EXTENDED);
    if (reti) { fprintf(stderr, "Could not compile regex for parentheses\n"); return -1; }

    reti = regcomp(&regex_braces, "^\\{[^A-Za-z{]*\\}", REG_EXTENDED);
    if (reti) { fprintf(stderr, "Could not compile regex for curly braces\n"); return -1; }

    reti = regcomp(&regex_brackets, "^\\[[^A-Za-z\\[]*\\]", REG_EXTENDED);
    if (reti) { fprintf(stderr, "Could not compile regex for square brackets\n"); return -1; }

    reti = regcomp(&regex_angles, "^<[^A-Za-z<]*>", REG_EXTENDED);
    if (reti) { fprintf(stderr, "Could not compile regex for angle brackets\n"); return -1; }
    // check rules
    char* location = &board[index];
    int parens = regexec(&regex_parens, location, 0, NULL, 0);
    int braces = regexec(&regex_braces, location, 0, NULL, 0);
    int brackets = regexec(&regex_brackets, location, 0, NULL, 0);
    int angles = regexec(&regex_angles, location, 0, NULL, 0);
    
    if(!parens || !braces || !brackets || !angles) {
        return 1;
    }
    return -1;
}

int wordLength(int index) {
    // check that we landed at an Aa-Zz (65->90 and 97->122)
    if(isalpha(board[index]) == 0) {
        return -1;
    }
    // go to right-most Aa-Zz
    int i = index;
    while(i+1 < BOARD_SIZE && isalpha(board[i+1])){
        i++;
    }
    return (i-index)+1;
}

char* wordFromIndex(int index) {
    int wordIndex = checkWord(index);
    if(wordIndex == -1) {
        return NULL;
    }
    int wlength = wordLength(index);
    char* word = malloc(sizeof(char)*(wlength+1));
    for(int i = 0; i < wlength; i++){
        word[i] = board[index+i];
    }
    word[wlength] = '\0';
    return word;
}

int choosePasswordIndex(){
    int i = randomNum(0, BOARD_SIZE-1);
    
    while(i+1 < BOARD_SIZE && !isalpha(board[i])){
        i++;
    }
    if(isalpha(board[i])) {
        return checkWord(i);
    } else {
        while(i-1 > -1 && !isalpha(board[i])){
            i--;
        }
        if(isalpha(board[i])) {
            return checkWord(i);
        }
    }
    return -1;
}

int findDudIndex(){
    int randDud = randomNum(0,w-1);
    int startingRandDud = randDud;
    int validIndexFound = 1;
    // if we land on invalid index
    if(wordIndexes[randDud] == passwordIndex || wordIndexes[randDud] == -1) {
        int validIndexFound = 0;
        // check left
        while(randDud-1 > -1 && !validIndexFound) {
            randDud--;
            if(wordIndexes[randDud] != passwordIndex && wordIndexes[randDud] != -1) {
                validIndexFound = 1;
                continue;
            }
        }
        // check right
        if(!validIndexFound) {
            randDud = startingRandDud;
        }
        while(randDud+1 < w && !validIndexFound) {
            randDud++;
            if(wordIndexes[randDud] != passwordIndex && wordIndexes[randDud] != -1) {
                validIndexFound = 1;
                continue;
            }
        }
    }
    if(validIndexFound) {
        int dudIndex = wordIndexes[randDud];
        wordIndexes[randDud] = -1;
        return dudIndex;
    }
    return -1;
}

int removeDud(){
    int dudIndex = findDudIndex();
    if(dudIndex == -1) {
        return -1;
    }
    int i = dudIndex;
    while(isalpha(board[i])) {
        board[i] = '.';
        i++;
    }
    return 0;
}

int symbolUsed(int index){
    for(int i = 0; i < u; i++) {
        if(usedSymbol[i]==index){
            return 1;
        }
    }
    return 0;
}

int strsim(char* a, char* b) {
    int sim = 0;
    int i = 0;
    while(a[i] != 0){
        if(a[i] == b[i]) {
            sim++;
        }
        i++;
    }
    return sim;
}

void clearScreen(){
    system("clear"); // For Linux/macOS
    // system("cls"); // For Windows
}

void refreshScreen() {
    clearScreen();
    printBoard();
}

// > entry denied. > x/n correct > dud removed. > tries reset. > password accepted.
int main()
{
    srand(time(NULL));
    fillBoardRandom();
    fillBoardWords();
    //printf("amount of words: %d\n", totalWords);
    passwordIndex = choosePasswordIndex();
    char* password = wordFromIndex(passwordIndex);
    fakeaddr = randomNum(0, 50000);
    
    board[0] = '<';
    board[1] = '>';
    
    printBoard();
    
    int endGame = 0;
    
    while(TEST){
        char input[50] = {0};
        char buffer[75] = {0};
        printf("> ");
        scanf("%s",input);
        if(strcmp(input, "exit()") == 0){
            printf("> EXITING...\n");
            return 0;
        }
        if(strcmp(input, "print()") == 0){
            refreshScreen();
            continue;
        }
        char* location = strstr(board, input);
        if (location != NULL) {
            int lindex = strlen(board)-strlen(location);
            //printf("> STRING FOUND AT %d.\n",lindex);
            int wordIndex = checkWord(lindex);
            if(wordIndex != -1) {
                // handle word case
                
                char* word = wordFromIndex(wordIndex);
                sprintf(buffer, "> %s", word);
                updateHistory(buffer);
                int likeness = strsim(word, password);
                if(likeness == strlen(password)) {
                    //printf("> PASSWORD ACCEPTED.\n");
                    updateHistory("> PASSWORD ACCEPTED.");
                    endGame = 1;
                } else {
                    //printf("> ENTRY DENIED.\n");
                    //printf("> LIKENESS= %d\n", likeness);
                    updateHistory("> ENTRY DENIED.");
                    sprintf(buffer, "> LIKENESS= %d", likeness);
                    updateHistory(buffer);
                    tries--;
                    if(tries < 1) {
                        updateHistory("> INIT LOCKOUT.");
                        sprintf(buffer, "> PWD: %s",password);
                        updateHistory(buffer);
                        endGame = 1;
                    }
                }
                free(word);
            } else {
                // handle symbol case
                //printf("word isn't valid.\n");
                sprintf(buffer, "> %s", input);
                updateHistory(buffer);
                int c = checkSymbol(lindex);
                //sprintf(buffer, "%d", c);
                //updateHistory(buffer);
                if(c != -1) {
                    int r = randomNum(0,4);
                    if(!r){
                        tries = 4;
                        updateHistory("> TRIES RESET.");
                    } else {
                        if (!removeDud() && !symbolUsed(lindex)){
                            usedSymbol[u++] = lindex;
                            updateHistory("> DUD REMOVED.");
                        } else {
                            updateHistory("> ERROR.");
                        }
                        
                    }
                } else {
                    updateHistory("> ERROR.");
                }
            }
        } else {
            //printf("> ERROR.\n");
            sprintf(buffer, "> %s", input);
            updateHistory(buffer);
            updateHistory("> ERROR.");
        }
        refreshScreen();
        if(endGame > 0) {
            return 0;
        }
    }
    
    free(password);
    regfree(&regex_parens);
    regfree(&regex_braces);
    regfree(&regex_brackets);
    regfree(&regex_angles);
    return 0;
}
