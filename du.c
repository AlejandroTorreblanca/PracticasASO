#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <pwd.h>
#include <libgen.h>
#include <getopt.h>
#define MAXARGS 15

int global;
int global_t;

void run_du(char** argv);

int
main(int argc, char *argv[])
{
    run_du(argv);
	return 0;
}

int display_info(const char *fpath, const struct stat *sb, int tflag,struct FTW *ftwbuf)
	{
		int aux=sb->st_blocks/2;
		global+=aux;
		return 0;           /* To tell nftw() to continue */
	}

int display_info_b(const char *fpath, const struct stat *sb, int tflag,struct FTW *ftwbuf)
	{
		printf("%jd %7jd %s\n",sb->st_blksize,sb->st_size, fpath);
		global+=sb->st_size;
		return 0;           /* To tell nftw() to continue */
	}

int display_info_t(const char *fpath, const struct stat *sb, int tflag,struct FTW *ftwbuf)
	{
		if(tflag == FTW_D)
			return 0;
		if(global_t>0)
			if(sb->st_size>global_t)
				global+=sb->st_size;
		if(global_t<0)
			if(sb->st_size<global_t*-1)
				global+=sb->st_size;
		return 0;           /* To tell nftw() to continue */
	}

int display_info_v(const char *fpath, const struct stat *sb, int tflag,struct FTW *ftwbuf)
	{		
		for(int n=0; n< ftwbuf->level; n++)
			printf("    ");
		if(tflag == FTW_D)
			printf("%s \n",fpath);
		else if(tflag == FTW_F)
		{
			printf("%s: %jd\n ",fpath,sb->st_size);
			global+=sb->st_size;
		}
		return 0;           /* To tell nftw() to continue */
	}	

void
run_du(char** argv)
{	
	int argc=0;
	int c;
	int flagh=0;
	int flagb=0;
	int flagt=0;
	int flagv=0;
	int flags=0;
	int tam=0;
	global=global_t=0;	
	
	while (argc<MAXARGS && argv[argc]!=NULL)
		argc++;
	while(1)
	{
		int option_index=0;
		c=getopt(argc, argv, "hbt:v");
		if(c==-1) 
			break;
		switch(c)
		{
			case 'h':
				flagh=1;
				break;
			case 'b':
				flagb=1;
				break;
			case 't':
				global_t=atol(argv[2]);
				if (global_t==0)
				{
					printf("Uso erróneo de la opción -t. Uso esperado [-t SIZE]\n");
				}
				else 
					flagt=1;
				break;
			case 'v':
				flagv=1;
				break;
			default:
				printf ("?? getopt returned character code 0%o ??\n", c);
		}
	}
	if (flagh)
		printf("Uso : du [-h] [- b] [ -t SIZE ] [-v ] [ FICHERO | DIRECTORIO ]...\nPara cada fichero , imprime su tamaño.\nPara cada directorio , imprime la suma de los tamaños de todos los ficheros de todos sus subdirectorios .\nOpciones :\n-b Imprime el tama ño ocupado en disco por todos los bloques del fichero .\n-t SIZE Excluye todos los ficheros más pequeños que SIZE bytes , si es positivo , o más grandes que SIZE bytes , si es negativo , cuando se procesa un directorio .\n-v Imprime el tamaño de todos y cada uno de los ficheros cuando se procesa un directorio .\n-h help\nNota : Todos los tamaños están expresados en bytes .\n");
	else if (flagb)
	{
		int indiceIni=optind;
		int i=indiceIni;
		int aux=0;
		struct stat sb;
		while(i<argc)
		{
			if (stat(argv[i], &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(argv[i], display_info_b, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("El tamaño es %d\n", global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:  
					printf("El tamaño del fichero es %7jd\n",sb.st_size);
		            break;
		       default:       printf("unknown?\n");
	                break;
		    }
			i++;			
		}
	}
	else if (flagt)
	{
		int indiceIni=optind;
		int i=indiceIni;
		struct stat sb;
		if(argc==i)			//caso en el que no se pasa ningun argumento, se podria optimizar codigo?
		{
			if (stat(".", &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(".", display_info_t, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("(D) .: %d\n", global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:  
		            break;
		       default:       printf("unknown?\n");
	                break;
		    }
		}
		while(i<argc)
		{
			if (stat(argv[i], &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(argv[i], display_info_t, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("(D) %s: %d\n",argv[i], global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:  
		            break;
		       default:       printf("unknown?\n");
	                break;
		    }
			i++;			
		}
	}
	else if(flagv)
	{
		int indiceIni=optind;
		int i=indiceIni;
		struct stat sb;
		if(argc==i)			//caso en el que no se pasa ningun argumento, se podria optimizar codigo?
		{
			if (stat(".", &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(".", display_info_v, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("(D) .: %d\n", global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:  
		            break;
		       default:       printf("El fichero no es un fichero regular ni un directorio\n");
	                break;
		    }
		}
		while(i<argc)
		{
			if (stat(argv[i], &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(argv[i], display_info_v, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("(D) %s %d\n",argv[i], global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:  
		            break;
		       default:       printf("El fichero no es un fichero regular ni un directorio\n");
	                break;
		    }
			i++;			
		}
	}
	else
	{
		int indiceIni=optind;
		int i=indiceIni;
		struct stat sb;
		
		if(argc==i)			//caso en el que no se pasa ningun argumento, se podria optimizar codigo?
		{
			if (stat(".", &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(".", display_info, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("(D) %s: %d\n",".", global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:
					printf("(F) %s: %jd\n",".", sb.st_size);
		            break;
		       default:       printf("El fichero no es un fichero regular ni un directorio\n");
	                break;
		    }
		}
		while(i<argc)
		{
			if (stat(argv[i], &sb) == -1) {
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	switch (sb.st_mode & S_IFMT)
			{
		       case S_IFDIR:  
				   	if (nftw(argv[i], display_info, 20, flags)== -1) {
						perror("nftw");
						exit(EXIT_FAILURE);
					}
					printf("(D) %s: %d\n",argv[i], global);
					global=0;					
					exit(EXIT_SUCCESS);
	               	break;
		       case S_IFREG:
					printf("(F) %s: %jd\n",argv[i], sb.st_size);
		            break;
		       default:       printf("El fichero no es un fichero regular ni un directorio\n");
	                break;
		    }
			i++;			
		}
	}
}














