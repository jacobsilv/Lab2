/* Match every k-character snippet of the query_doc document
	 among a collection of documents doc1, doc2, ....

	 ./rkmatch snippet_size query_doc doc1 [doc2...]

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>


#include "bloom.h"

enum algotype { EXACT=0, SIMPLE, RK, RKBATCH};

/* a large prime for RK hash (BIG_PRIME*256 does not overflow)*/
long long BIG_PRIME = 5003943032159437; 

/* constants used for printing debug information */
const int PRINT_RK_HASH = 5;
const int PRINT_BLOOM_BITS = 160;


/* calculate modulo addition, i.e. (a+b) % BIG_PRIME */
long long
madd(long long a, long long b)
{
	assert(a >= 0 && a < BIG_PRIME && b >= 0 && b < BIG_PRIME);
	return ((a+b)>BIG_PRIME?(a+b-BIG_PRIME):(a+b));
}

/* calculate modulo substraction, i.e. (a-b) % BIG_PRIME*/
long long
mdel(long long a, long long b)
{
	assert(a >= 0 && a < BIG_PRIME && b >= 0 && b < BIG_PRIME);
	return ((a>b)?(a-b):(a+BIG_PRIME-b));
}

/* calculate modulo multiplication, i.e. (a*b) % BIG_PRIME */
long long
mmul(long long a, long long b)
{
	assert(a >= 0 && a < BIG_PRIME && b >= 0 && b < BIG_PRIME);
	/* either operand must be no larger than 256, otherwise, there is danger of overflow*/
	assert(a <= 256 || b <= 256); 
	return ((a*b) % BIG_PRIME);
}
/*
void
print_chunk(char *buf, int len)
{
	for (int i = 0; i < len;i++) {
		printf("%c", buf[i]);
	}
	printf("\n");
}
*/
/* read the entire content of the file 'fname' into a 
	 character array allocated by this procedure.
	 Upon return, *doc contains the address of the character array
	 *doc_len contains the length of the array
	 */
void
read_file(const char *fname, unsigned char **doc, int *doc_len) 
{

	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("read_file: open ");
		exit(1);
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		perror("read_file: fstat ");
		exit(1);
	}

	*doc = (unsigned char *)malloc(st.st_size);
	if (!(*doc)) {
		fprintf(stderr, " failed to allocate %d bytes. No memory\n", (int)st.st_size);
		exit(1);
	}

	int n = read(fd, *doc, st.st_size);
	if (n < 0) {
		perror("read_file: read ");
		exit(1);
	}else if (n != st.st_size) {
		fprintf(stderr,"read_file: short read!\n");
		exit(1);
	}
	
	close(fd);
	*doc_len = n;
}

/* The normalize procedure normalizes a character array of size len 
   according to the following rules:
	 1) turn all upper case letters into lower case ones
	 2) turn any white-space character into a space character and, 
	    shrink any n>1 consecutive whitespace characters to exactly 1 whitespace

	 When the procedure returns, the character array buf contains the newly 
     normalized string and the return value is the new length of the normalized string.

     hint: you may want to use C library function isupper, isspace, tolower
     do "man isupper"
*/
int
normalize(unsigned char *buf,	/* The character array contains the string to be normalized*/
	int len	    /* the size of the original character array */)
{
	/*
	if (len == 0 || buf == NULL) {
		return 0;
	}*/
	


	//char *string = (char*) malloc( sizeof(char)*(len+1));//new
	//memcpy(string, buf, len+1);//new
	//char* string2 = string;
	//char *newBuf = (char*) malloc( sizeof(char)*(i+1));
	
	//create  a char pointer that points to buf
	char *string = buf;

	//create  a char pointer that points to string which points to buf
	char *newString = string;
	//char *beg = newString;
	
	// removes any white spaces that happen at the beginning of the string
	while(isspace(*string)) string++;

	//iterate through the string that is buf
	for (; *string; string++){ 
	//while(*string!='\0'){
		
		 //assign the values that is a char to newString
		*newString++ = *string;
		
		 //a space was found
		if(isspace(*string)){

			// run the loop once no matter what
			do { 
				
				// shift the string once to the right
				++string; 

			//continue the loop till the string is not pointing to a white space
			}while (isspace (*string)); 

			 // have the pointer point one value before (a single space)
			--string;
		}
		
		if (string+1 == '\0' && isspace(*string)) {
			//string--;
			*string = '\0';
			
		}
		//string++;

	}
	
	//if (isspace(*(newString--))) newString--;
	
	*(newString++) = '\0'; // assign the last value to null

	int i = 0;
	
	while(buf[i] != '\0'){ //iterate till buf reaches a null value
		if (isupper(buf[i])){ // find values that are upper case
			buf[i] = tolower(buf[i]); // assign that position to be that char but lower case
		}
		i++; //increment to the next value
	}
	return i;
	


	//char *newBuf = (char*) malloc( sizeof(char)*(i+1));
	
	/*
	while(string[i] != '\0'){
		if (isupper(string[i])){
			string[i] = tolower(string[i]);
		}
		i++;
	}
	*/
	
	//memcpy(newBuf, buf, i+1);//new
	//free(string2);
	//*buf = newBuf;



	//printf("\n%s\n", buf);
	/*
	char *newBuf = (char*) malloc( sizeof(char)*(i+1));
	while(*beg != '\0'){
		*newBuf = *beg;
		beg++;
	}
	buf = newBuf;
	*/
	// return the increment of how ever long buf happened to be when changing chars to lower case
	//return i;
	/*
	int backtrack = 0;
	int i;
	for (i=0; i <len; i++){
		if(*(buf+i) > 40 && *(buf+i) < 91){
			*(buf+i-backtrack) = *(buf+i) +32;
		}
		else if (isspace(*(buf+i))){
			if (isspace(*(buf+i-1))){
				backtrack++;
			}
			*(buf+i-backtrack) = ' ';
		}
		else{
			*(buf+i-backtrack) = *(buf+i);
		}
	}
	*(buf+len-backtrack-1) = '\0';
	return len-backtrack;*/
	
	/*
	// writes/real length
        int counter = 0;
        // jumps
        int i = 0;
        // to eliminate additional  migraines
        while (isupper(*(buf + i))) {
                *(buf + i) = tolower(*(buf + i));
        }
        i = 0;
        for (i; i < len; i++) {
                // if current whitespace is preceded by a whitespace            
                if ((counter != 0) && (isspace(*(buf + i))) && (isspace(*(buf + (i - 1))))) {
                        i++;
                        while (isspace(*(buf + i))) {
                                i++;
                        }
                        *(buf + counter) = *(buf + i);
                        counter++;
                }
                // if current whitespace is not preceded by a whitespace
                else if ((counter != 0) && (isspace(*(buf + i))) && !(isspace(*(buf + (i - 1))))) {
                        *(buf + counter) = ' ';
                        counter++;
                }
                // if current character is not a space
                else if (counter != 0) {
                        *(buf + counter) = *(buf + i);
                        counter++;
                }
                // bypass: (counter = 0 and current character is a whitespace), and others? RECORD
        }
        // gets rid of trailing whitespaces
        while (isspace(*(buf + (counter - 1)))) {
                *(buf + (counter - 1)) = '\0';
                counter--;
        }
        return counter;*/

}

int
exact_match(const unsigned char *qs, int m, 
        const unsigned char *ts, int n)
{
	int i = 0;
	for(i = 0; i < n; i++) {
		if(qs[i] != ts[i]) { // makes sure the values match but if they dont then return (false) they didnt match
			return 0; 
		}
	}

	return 1;
	/*
	if(strcmp((const char *) qs,  (const char *) ts) == 0) {
		return 1;
	}
	return 0;*/
}

/* check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring 
	 If so, return 1. Else return 0
	 You may want to use the library function strncmp
	 */
int
simple_substr_match(const unsigned char *ps,	/* the query string */
	int k,/* the length of the query string */
	const unsigned char *ts,/* the document string (Y) */ 
	int n/* the length of the document Y */)
{

	int i;
	// make sure that the substring isnt bigger than the string
	if ( k > n){
		return 0;
	}

	// iterate till you reach a point in the string where the rest of the string is shorter than the substring
	for (i = 0; i<n-k+1; i++){
		if(strncmp(ps, ts, k)==0){//compare the string with the substring
			return 1;
		}
		ts++; //move the string pointer over one
	}
	return 0;



	/*
	for (i = 0; i < n-k+1; i++){
		for (j = i; j <= i+k; j++) {
			if (ts[j] == *ps){
				ps++;
			}
			else if (*ps == '\0'){
				return 1;
			}
			else {
				break;
			}
		}
		ps = ps_copy;
	}
	*/


}


/* hash value converter for strings*/
/*
unsigned long long int 
hashing(const unsigned char* string, int k){
	unsigned long long int s;
	long double power;
	int i;
	for (i = 0; i <k; i++ ){
		power =  pow((double)256, (double)(k- i + 1));

		s += *(unsigned int *)string * (unsigned int) power;	
		string++;
	}
	return s;
}*/


/*
This is a function that will return the number that is calculated by the two parameter.
The first paramter is a number and the second parameter is the power the number needs
to be raise to. This functions differs from pow because it uses the multiplicaiton method 
she provided.
*/
long long
power(long long number, long long power){

	long long result = 1;
	int i;

	for(i=0; i<power;i++){//increment the number of times you are raise to the power of
		result = mmul(result, number); //multiples the previous value collected by number
	}
	return result;
}


/* Check if a query string ps (of length k) appears 
in ts (of length n) as a substring using the rabin-karp algorithm
If so, return 1. Else return 0

In addition, print the hash value of ps (on one line)
as well as the first 'PRINT_RK_HASH' hash values of ts (on another line)
Example:
	$ ./rkmatch -t 2 -k 20 X Y
	4537305142160169
	1137948454218999 2816897116259975 4720517820514748 4092864945588237 3539905993503426 
	2021356707496909
	1137948454218999 2816897116259975 4720517820514748 4092864945588237 3539905993503426 
	0 chunks matched (out of 2), percentage: 0.00

Hint: Use "long long" type for the RK hash value.  Use printf("%lld", x) to print 
out x of long long type.
*/
int
rabin_karp_match(const unsigned char *ps,	/* the query string */
	int k,/* the length of the query string */
	const unsigned char *ts,/* the document string (Y) */ 
	int n/* the length of the document Y */ )
{
	// make sure you values are not null
	
	if (ps==NULL || ts == NULL){
		return 0;
	}

	
	int found = 0;// value that determines whether the substring was found in the string
	long long initSHV = 0; // the initial hash value of the string from [0..k-1]
	long long SBHV = 0; // this is the substring hash value
	long long base = 256; 

	int i;
	for (i=0; i < k; i++){ 
		//SBHV += (unsigned int) ps[i] * power(256, k-i-1);
		//initSHV += (unsigned int) ts[i]*  power(256, k-i-1);
		
		// you will multiple the char by the proper base then add it to the previously found values
		SBHV = madd(SBHV, mmul( ps[i] , power(base, k-i-1)));

		initSHV= madd(initSHV, mmul( ts[i], power(base, k-i-1)));
	}
	//print initial values
	printf("%lld\n",SBHV); 
	printf("%lld ", initSHV);

	// the substring was found in the first elements
	if (initSHV == SBHV ){
		found = 1;
	}

	long long last_SHV = initSHV; // make a variable for the previously found hash value
	long long new_SHV = 0; //the newest hash value
	int j;

	
	// for loop this the characters left in string as less than the substring
	for (j = 0; j < n-k+1; j++){
		//new_SHV = (256*(last_SHV-power(256, k-1) * ts[j]) + ts[j+k]);

		// the formula given to calculate the rolling hash value
		new_SHV =  madd(mmul(mdel(last_SHV, mmul( power(base, k-1),  ts[j])), base), ts[j+k]);
		
		// test to see if the new hash value matches the substring hash value
		if (new_SHV == SBHV ){
			found = 1;
		}
		// print the next four string hash values because the initial string hash value was already printed
		if (j < 4){
			printf("%lld ", new_SHV);
		} 
		//else if (j==3) printf("%lld", new_SHV);
		
 		// set the last hash value to the new one so that we can calculate the next hash value in the next iteration
		last_SHV = new_SHV;
	}
	printf("\n");
	
	return found;

}


long long 
rabin_hash (const char *to_encode, int len){
	long long hash =0;
	int i;

	for (i = 0; i<len; i++){
		hash = madd(*(to_encode + i), mmul(hash, 256));
	}
	return hash;
}


/* Allocate a bitmap of bsz bits for the bloom filter (using bloom_init) and insert 
 all m/k RK hashes of qs into the bloom filter (using bloom_add).  Compute each of the n-k+1 RK hashes 
 of ts and check if it's in the filter (using bloom_query). 
 This function returns the total number of matched chunks. 

 For testing purpose, you should print out the first PRINT_BLOOM_BITS bits 
 of bloom bitmap in hex after inserting all m/k chunks from qs.
 */
int
rabin_karp_batchmatch(int bsz, /* size of bitmap (in bits) to be used */
    int k,/* chunk length to be matched */
    const unsigned char *qs, /* query docoument (X)*/
    int m,/* query document length */ 
    const unsigned char *ts, /* to-be-matched document (Y) */
    int n /* to-be-matched document length*/)
{
	
	if (n < k){
		return 0;
	}
	bloom_filter filter = bloom_init(bsz);
	
	long long hash_factor = 1;
	int i;
	int j;
	
	for (i=0; i<k-1;i++){
		hash_factor = mmul(hash_factor, 256);
	}

	int score = 0;
	for (i=0; i<m/k; i++){
		bloom_add(filter, rabin_hash(qs + i * k, k));
	}

	bloom_print(filter, PRINT_BLOOM_BITS);


	long long rolling_hash = rabin_hash(ts, k);

	for (i=0; i < n-k+1; i++){
		if (bloom_query(filter, rolling_hash)){
			for (j=0; j<m/k;j++){
				if (!strncmp(qs+j*k, ts+i, k)){
					score++;
					break;
				}
			}
		}
		rolling_hash = madd(mmul(mdel(rolling_hash, mmul(*(ts+i), hash_factor)), 256), *(ts+k+i));
	}
	return score;

	

	
}

int 
main(int argc, char **argv)
{
	int k = 20; /* default match size is 20*/
	int which_algo = SIMPLE; /* default match algorithm is simple */

	
	/* Refuse to run on platform with a different size for long long*/
	assert(sizeof(long long) == 8);

	/*getopt is a C library function to parse command line options */
	int c;
	while ((c = getopt(argc, argv, "t:k:q:")) != -1) {
	       	switch (c) {
			case 't':
				/*optarg is a global variable set by getopt() 
					it now points to the text following the '-t' */
				which_algo = atoi(optarg);
				break;
			case 'k':
				k = atoi(optarg);
				break;
			case 'q':
				BIG_PRIME = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Valid options are: -t <algo type> -k <match size> -q <prime modulus>\n");
				exit(1);
	       	}
       	}

	/* optind is a global variable set by getopt() 
		 it now contains the index of the first argv-element 
		 that is not an option*/
	if (argc - optind < 1) {
		printf("Usage: ./rkmatch query_doc doc\n");
		exit(1);
	}

	unsigned char *qdoc, *doc; 
	int qdoc_len, doc_len;
	/* argv[optind] contains the query_doc argument */
	read_file(argv[optind], &qdoc, &qdoc_len); 
	qdoc_len = normalize(qdoc, qdoc_len);

	/* argv[optind+1] contains the doc argument */
	read_file(argv[optind+1], &doc, &doc_len);
	doc_len = normalize(doc, doc_len);

	int num_matched;
	switch (which_algo) {
            case EXACT:
                if (exact_match(qdoc, qdoc_len, doc, doc_len)) 
                    printf("Exact match\n");
                else
                    printf("Not an exact match\n");
                break;
	   
	    case SIMPLE:
	       	/* for each chunk of qdoc (out of qdoc_len/k chunks of qdoc, 
		 * check if it appears in doc as a substring */
		num_matched = 0;
		 for (int i = 0; (i+k) <= qdoc_len; i += k) {
			 if (simple_substr_match(qdoc+i, k, doc, doc_len)) {
				 num_matched++;
			 }
			
		 }
		 printf("%d chunks matched (out of %d), percentage: %.2f\n", \
				 num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
		 break;
	   
	    case RK:
		 /* for each chunk of qdoc (out of qdoc_len/k in total), 
		  * check if it appears in doc as a substring using 
		  * the rabin-karp substring matching algorithm */
		 num_matched = 0;
		 for (int i = 0; (i+k) <= qdoc_len; i += k) {
			 //print_chunk(qdoc+i, k);
			 if (rabin_karp_match(qdoc+i, k, doc, doc_len)) {
				 num_matched++;
			 }
		 }
		 printf("%d chunks matched (out of %d), percentage: %.2f\n", \
				 num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
		 break;
	   
	    case RKBATCH:
		 /* match all qdoc_len/k chunks simultaneously (in batch) by using a bloom filter*/
		 num_matched = rabin_karp_batchmatch(((qdoc_len*10/k)>>3)<<3, k, \
				 qdoc, qdoc_len, doc, doc_len);
		 printf("%d chunks matched (out of %d), percentage: %.2f\n", \
				 num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
		 break;
	   
	    default :
		 fprintf(stderr,"Wrong algorithm type, choose from 0 1 2 3\n");
		 exit(1);
	}

	free(qdoc);
	free(doc);

	return 0;
}
