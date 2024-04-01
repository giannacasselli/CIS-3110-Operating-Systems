#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

//define for dynamic alloc/realloc
#define MORE_MEM 1024
//shared data between threads
pthread_t threads[20];
int thread_count=0;
FILE *summary;
//pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t summary_mutex=PTHREAD_MUTEX_INITIALIZER;
int num_files=0;
int spell_errs=0;
int active_threads=0;
//structures for thread info
typedef struct{
    char dictionary[100];
    char text[100];
}FileName;

typedef struct{
    char **words;
    size_t num_words;
}Parsed;
//check to see if files are valid
int FileCheck(char dic[100], char txt[100]){
    FILE *file_dic;
    FILE *file_txt;
    file_dic=fopen(dic,"r");
    file_txt=fopen(txt,"r");
    if(file_dic==NULL){
        printf("%s is an invalid dictionary. Thread will not be created.\n",dic);
        return 0;
    }
    if(file_txt==NULL){
        printf("%s is an invalid text file. Thread will not be created.\n",txt);
        return 0;
    }
    fclose(file_dic);
    fclose(file_txt);
    return 1;
}

//parse function
//accepts file name
//returns struct of array of words and word count
//Where first letter of each word is capitalized
//Every other letter is lowercase
//For easy comparison in spellchecking function
Parsed parse(char filename[100]){
    FILE *file;
    file=fopen(filename,"r");
    char **words;
    //allocate initial memory
    words=malloc(sizeof(char *)*MORE_MEM);
    //size variables for realloc increases
    size_t word_count=0;
    size_t char_count=0;
    char c;
    do{
        //continuously get chars
        c=fgetc(file);
        if(feof(file)|| c==EOF){
            //if end of file occurs after a word with no null terminator or newline
            if(char_count!=0){
                //adjust word size and increse by 1 to tack on null terminator
                words[word_count]=realloc(words[word_count],char_count+1);
                words[word_count][char_count]='\0';
                //increase total word count and reset char count
                word_count++;
                char_count=0;
            }
            //exit loop -> file has been read
            break;
        }
        //if end of a word
        else if(!(((c>='A' && c<='Z')||(c>='a' && c<='z')))){
            //if previous chars exist
            if(char_count!=0){
                //if previous chars in the word exist and there is an apostrophe
                //e.g "Don't"
                //add the apostrophe to the word
                //need to improve to include non-standard apostrophe types
                //i.e. not included in ASCII but in Unicode
                if(c=='\''){
                     words[word_count][char_count]=c;
                    //increase char count
                    char_count++;
                    //if more memory required for chars, reallocate
                    if(char_count%MORE_MEM==0){
                    size_t new = char_count + MORE_MEM;
                    words[word_count]=realloc(words[word_count],new);
                    }
                }
                //if not an apostrophe but previous characters exist
                else{
                    //if the previous character is meant to be a single quote
                    //and not an apostrophe
                    //decrease char count to overwrite single quote with null terminator
                    if(words[word_count][char_count-1]=='\''){
                        char_count--;
                    }
                    //readjust word size, increase by one and append null terminator
                    words[word_count]=realloc(words[word_count],char_count+1);
                    words[word_count][char_count]='\0';
                    //increase word count and reset char count
                    word_count++;
                    char_count=0;
                    //if all the allocated memory for the words has been used
                    //reallocate with more
                    if(word_count%MORE_MEM==0){
                        size_t new = word_count+MORE_MEM;
                        words=realloc(words,sizeof(char *)*new);
                    }
                }
            }
            //if a numeric digit is encountered
            if((c>=48) && (c<=57)){
                c=fgetc(file);
            }
        }
        //if not eof or end of word
        else{ 
            //if new word
            if(char_count==0){
                //if single quote, double quote or open bracket
                if((c=='\'')||(c=='"')||(c=='(')||(c=='[')||(c=='{')){
                    c=fgetc(file);
                }
                //allocate mem
                words[word_count]=malloc(MORE_MEM);
                //first char must be capitalized to match format
                if((c>=97 && c<=122)){
                    c=c-32;
                }
            }
            //all following chars must be lower case
            //error handling for numbers and special chars must be implemented yet
            if(c<90 && char_count!=0 && c!='\''){
                c=c+32;
            }
            //set character in word
            words[word_count][char_count]=c;
            //increase char count
            char_count++;
            //if more memory required for chars, reallocate
            if(char_count%MORE_MEM==0){
                size_t new = char_count + MORE_MEM;
                words[word_count]=realloc(words[word_count],new);
            }
        }
        
    }while(1);
    //final realloc
    words=realloc(words,sizeof(char *)*word_count);
    fclose(file);
    Parsed val={words, word_count};
    int i;
    for(i=0;i<word_count;i++){
        strcpy(val.words[i],words[i]);
        free(words[i]);
    }
    free(words);
    val.num_words=word_count;
    return val;

}

void *spellcheck(void *args){
    //use mutex to update active thread count
    pthread_mutex_lock(&mutex2);
    active_threads++;
    pthread_mutex_unlock(&mutex2);
    FileName *arg=(FileName *)args;
    //copy parsed word arrays into struct
    size_t num_dic_words=0;
    size_t num_text_words=0;
    Parsed parse_text=parse(arg->text);
    Parsed parse_dic=parse(arg->dictionary);
    num_dic_words=parse_dic.num_words;
    num_text_words=parse_text.num_words;
    char **dic_words=parse_dic.words;
    char **text_words=parse_text.words;
    int i;
    int j;
    //initialize integer array corresponding to each word
    //initialize each value to 0
    //initialize error count to total number of words
    //if a match is detected, change value to 1
    //decrease error count
    int spell[num_text_words];
    int errs=num_text_words;
    //printf("working\n");
    for(i=0;i<num_text_words;i++){
        spell[i]=0;
        //printf("working1\n");
        for(j=0;j<num_dic_words;j++){
            if(strcmp(text_words[i],dic_words[j])==0){
                spell[i]=1;
                errs--;
                break;
            }
        }
    }
    //create array to store mispelled words
    char bad_words[errs][45];
    int count=0;
    for(i=0;i<num_text_words;i++){
        if(spell[i]==0){
            strcpy(bad_words[count],text_words[i]);
            count++;
        }

    }
    //initialize integer array to hold duplicate occurences
    int num_times[errs];
    int duplicate=0;
    //iterate over mispelled words
    for(i=0;i<errs;i++){
        //initialize each word's occurence to 1
        num_times[i]=1;
        //for each word, iterate over the rest of the mispelled array
        for(j=i+1;j<errs+1;j++){
            //if the current word has not been marked as a duplicate
            if(strcmp(bad_words[i],"duplicate")!=0){
                //if one of the following words matches the current word
                if(strcmp(bad_words[i],bad_words[j])==0){
                    //increase the number of occurences for that word
                    num_times[i]++;
                    //mark the duplicate instance of the word as duplicate
                    strcpy(bad_words[j],"duplicate");
                    //increase the total number of duplicates
                    duplicate++;
                }
            }
            //if the current word is marked as a duplicate
            //change occurence number to 0 as it has already been counted
            //in the first instance of the word
            else{
                num_times[i]=0;
            }
        }
    }
    //create variables for unique words
    int unique=errs-duplicate;;
    char unique_words[unique][45];
    int unique_count[unique];
    int k=0;
    //put all unique mispelled words into one array
    for(i=0;i<errs;i++){
        if(strcmp(bad_words[i],"duplicate")!=0){
            strcpy(unique_words[k],bad_words[i]);
            unique_count[k]=num_times[i];
            k++;
        }
    }
    //final output stuff
    char three_words[3][45];
    int three_times[3];
    int temp1=0;
    int temp2=0;
    int temp3=0;
    for(i=0;i<unique;i++){
        if(unique_count[i]>temp1){
            strcpy(three_words[2],three_words[1]);
            strcpy(three_words[1],three_words[0]);
            strcpy(three_words[0],unique_words[i]);
            three_times[2]=three_times[1];
            three_times[1]=three_times[0];
            three_times[0]=unique_count[i];
            temp3=temp2;
            temp2=temp1;
            temp1=unique_count[i];
        }
        else if(unique_count[i]>temp2){
            strcpy(three_words[2],three_words[1]);
            strcpy(three_words[1],unique_words[i]);
            three_times[2]=three_times[1];
            three_times[1]=unique_count[i];
            temp3=temp2;
            temp2=unique_count[i];
        }
        else if(unique_count[i]>temp3){
            strcpy(three_words[2],unique_words[i]);
            three_times[2]=unique_count[i];
            temp3=unique_count[i];
        }
    }
    //mutex stuff to write to shared file
    pthread_mutex_lock(&summary_mutex);
    summary=fopen("tcassell_A2.out","a");
    if(summary==NULL){
        printf("Error generating summary. Thread will exit.\n");
        pthread_exit(NULL);
    }
    fprintf(summary,"%s %d %s %s %s\n",arg->text, errs, three_words[0],three_words[1],three_words[2]);
    fclose(summary);
    pthread_mutex_unlock(&summary_mutex);
    pthread_mutex_lock(&mutex2);
    num_files++;
    spell_errs=spell_errs+errs;
    active_threads--;
    pthread_mutex_unlock(&mutex2);
    //exit thread
    pthread_exit(NULL);

    return NULL;
}

//recursive menu func
int menu (int select,FileName *files){
    int num=0;
    if(select==0){
        printf("---------- Main Menu ----------\n");
        printf("|                              |\n");
        printf("|   Please select an option    |\n");
        printf("| 1 - Begin new spellcheck     |\n");
        printf("| 2 - Exit                     |\n");
        printf("|                              |\n");
        printf("-------------------------------\n");
        if(scanf(" %d",&num)!=1){
            while(getchar()!='\n'){}
            num=3;
        }
        printf("\n");
        return menu(num,files);

    }
    else if(select==1){
        char dic[100];
        char txt[100];
        printf("-----------New Spellcheck-----------\n");
        printf("|                                   |\n");
        printf("| Please enter the dictionary file  |\n");
        printf("| followed by the text file,        |\n");
        printf("| separated by the 'enter' key      |\n");
        printf("|                                   |\n");
        printf("| If you wish to return to the main |\n");
        printf("| menu, enter 0                     |\n");
        printf("-------------------------------------\n");
        scanf(" %s",dic);
        printf("\n");
        if(strcmp(dic,"0")==0){
            return menu(0,files);
        }
        scanf(" %s",txt);
        printf("\n");
        if(strcmp(txt,"0")==0){
            return menu(0,files);
        }
        if(FileCheck(dic,txt)){
            strcpy(files->dictionary,dic);
            strcpy(files->text,txt);
            if(thread_count<20){
                pthread_create(&threads[thread_count],NULL,spellcheck,(void *)files);
                thread_count++;
            }
            else{
                printf("Too many threads! Please exit at main menu to receive summary\n");
            }
            return menu(0,files);
        }
        else{
            printf("Something went wrong opening files.\nReturning to main menu.\n");
            return menu(0,files);
        }
        printf("Input received successfully.\nReturning to main menu.\n");
        printf("\n");
        return menu(0,files);

    }
    else if(select==2){
        return 0;
    }
    else{
        printf("Invalid menu selection. Please try again.\n");
        return menu(0,files);
    }

}

int main(int argc, char *argv[])
{
    FileName *files=malloc(sizeof(FileName));
    strcpy(files->dictionary,"init");
    strcpy(files->text,"init");
    int flag=1;
    flag=menu(0,files);
    free(files);
    if(active_threads>0){
        printf("%d tasks still running.\n",active_threads);
    }
    else{
        printf("All tasks have completed.\n");
    }
    if(flag==0){
        pthread_mutex_lock(&summary_mutex);
        summary = fopen("tcassell_A2.out", "r");
        char line[300];
        int line_count = 0;
        if (summary == NULL)
        {
            printf("Could not open summary file\n");
            return 1;
        }
        while (fgets(line, sizeof(line), summary) != NULL)
        {
            line_count++;
        }
        char buf[100];
        char sum[line_count * 3][100];
        int i, j;
        for(i=0;i<line_count*3;i++){
            strcpy(sum[i],"duplicate");
        }
        int buff;
        int a = 0;
        fclose(summary);
        summary = fopen("tcassell_A2.out", "r");
        while (fgets(line, sizeof(line), summary) != NULL)
        {
            sscanf(line, "%s %d %s %s %s", buf, &buff, sum[a], sum[a + 1], sum[a + 2]);
            if(buff==0){
                strcpy(sum[a],"duplicate");
                strcpy(sum[a+1],"duplicate");
                strcpy(sum[a+2],"duplicate");
            }
            a += 3;
        }
        // same code from spellchecking function
        int errs = line_count * 3;
        int num_times[errs];
        int duplicate = 0;
        for (i = 0; i < errs; i++)
        {
            num_times[i] = 1;
            for (j = i + 1; j < errs + 1; j++)
            {
                if (strcmp(sum[i], "duplicate") != 0)
                {
                    if (strcmp(sum[i], sum[j]) == 0)
                    {
                        num_times[i]++;
                        strcpy(sum[j], "duplicate");
                        duplicate++;
                    }
                }
                else
                {
                    num_times[i] = 0;
                }
            }
        }
        int unique = errs - duplicate;
        char unique_words[unique][45];
        int unique_count[unique];
        int k = 0;
        for(i=0;i<unique;i++){
            strcpy(unique_words[i],"---");
            unique_count[i]=0;
        }
        for (i = 0; i < errs; i++)
        {
            if (strcmp(sum[i], "duplicate") != 0)
            {
                strcpy(unique_words[k], sum[i]);
                unique_count[k] = num_times[i];
                k++;
            }
        }
        char three_words[3][45];
        int three_times[3];
        for(i=0;i<3;i++){
            strcpy(three_words[i],"---");
            three_times[i]=0;
        }
        int temp1 = 0;
        int temp2 = 0;
        int temp3 = 0;
        for (i = 0; i < unique; i++)
        {
            if (unique_count[i] > temp1)
            {
                strcpy(three_words[2], three_words[1]);
                strcpy(three_words[1], three_words[0]);
                strcpy(three_words[0], unique_words[i]);
                three_times[2] = three_times[1];
                three_times[1] = three_times[0];
                three_times[0] = unique_count[i];
                temp3 = temp2;
                temp2 = temp1;
                temp1 = unique_count[i];
            }
            else if (unique_count[i] > temp2)
            {
                strcpy(three_words[2], three_words[1]);
                strcpy(three_words[1], unique_words[i]);
                three_times[2] = three_times[1];
                three_times[1] = unique_count[i];
                temp3 = temp2;
                temp2 = unique_count[i];
            }
            else if (unique_count[i] > temp3)
            {
                strcpy(three_words[2], unique_words[i]);
                three_times[2] = unique_count[i];
                temp3 = unique_count[i];
            }
        }
        fclose(summary);
        pthread_mutex_unlock(&summary_mutex);
        if(argc==2){
            if(strcmp(argv[1],"-l")==0){
                //critical section for appending to summary file
                //tasks could still be updating file after the check
                pthread_mutex_lock(&summary_mutex);
                summary=fopen("tcassell_A2.out","a");
                fprintf(summary,"Number of files processed: %d\n",num_files);
                fprintf(summary,"Number of spelling errors: %d\n",spell_errs);
                fprintf(summary,"Three most common mispellings: %s (%d times), %s (%d times), %s (%d times)\n",
                three_words[0], three_times[0], three_words[1], three_times[1], three_words[2], three_times[2]);
                fclose(summary);
                pthread_mutex_unlock(&summary_mutex);
            }
        }
        else{
            printf("Number of files processed: %d\n",num_files);
            printf("Number of spelling errors: %d\n",spell_errs);
            printf("Three most common mispellings: %s (%d times), %s (%d times), %s (%d times)\n",
            three_words[0], three_times[0], three_words[1], three_times[1], three_words[2], three_times[2]);
        }

    }

    return 0;
}