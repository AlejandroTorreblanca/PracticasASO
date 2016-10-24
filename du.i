#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <pwd.h>
#include <libgen.h>
#include <getopt.h>
#define MAXARGS 15

void run_du(char** argv);

int
main(int argc, char *argv[])
{
    run_du(argv);
	return 0;
}

void
run_du(char** argv)
{	
	int argc=0;
	int c;
	int flag_h=0;
	int flag_b=0;
	int flag_t=0;
	int flag_v=0;
	int flags=0;
	int tam=0;
	int tamTotal=0;

	int 
	display_info(const char *fpath, const struct stat *sb, int tflag,struct FTW *ftwbuf)
	{
		if(tflag==FTW_D)
		{
			if (flag_v)
			{
				for(int n=0; n< ftwbuf->level; n++)
				printf("    ");
				printf("%s \n",fpath);
			}
		}
		else if (tflag==FTW_F)
		{
			if (flag_b)
			{
				if (flag_t)
				{
					if(tam>0)
						if((sb->st_blocks)*512>tam)
						{
							tamTotal+=(sb->st_blocks)*512;
							if (flag_v)
							{
								for(int n=0; n< ftwbuf->level; n++)
									printf("    ");
								printf("%s: %jd\n ",fpath,(sb->st_blocks*512));
							}
						}
					if(tam<0)
						if((sb->st_blocks)*512<tam*(-1))
						{
							tamTotal+=(sb->st_blocks)*512;
							if (flag_v)
							{
								for(int n=0; n< ftwbuf->level; n++)
									printf("    ");
								printf("%s: %jd\n ",fpath,(sb->st_blocks*512));
							}
						}
				}
				else 
				{
					tamTotal+=(sb->st_blocks)*512;
					if (flag_v)
					{
						for(int n=0; n< ftwbuf->level; n++)
							printf("    ");
						printf("%s: %jd\n ",fpath,(sb->st_blocks*512));
					}
				}
				
			}
			else
			{
				if (flag_t)
				{
					if(tam>0)
						if(sb->st_size>tam)
						{
							tamTotal+=sb->st_size;
							if (flag_v)
							{
								for(int n=0; n< ftwbuf->level; n++)
									printf("    ");
								printf("%s: %jd\n ",fpath,sb->st_size);
							}
						}
					if(tam<0)
						if(sb->st_size<tam*(-1))
						{
							tamTotal+=sb->st_size;
							if (flag_v)
							{
								for(int n=0; n< ftwbuf->level; n++)
									printf("    ");
								printf("%s: %jd\n ",fpath,sb->st_size);
							}
						}
				}
				else 
				{
					tamTotal+=sb->st_size;
					if (flag_v)
					{
						for(int n=0; n< ftwbuf->level; n++)
							printf("    ");
						printf("%s: %jd\n ",fpath,sb->st_size);
					}
				}
			}
			
		}
		return 0;           
	}

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
				flag_h=1;
				break;
			case 'b':
				flag_b=1;
				break;
			case 't':
				tam=atol(argv[2]);
				if (tam==0)
				{
					printf("Uso erróneo de la opción -t. Uso esperado [-t SIZE]\n");
				}
				else 
					flag_t=1;
				break;
			case 'v':
				flag_v=1;
				break;
			default:
				printf ("?? getopt returned character code 0%o ??\n", c);
		}
	}
	if (flag_h)
		printf("Uso : du [-h] [- b] [ -t SIZE ] [-v ] [ FICHERO | DIRECTORIO ]...\nPara cada fichero , imprime su tamaño.\nPara cada directorio , imprime la suma de los tamaños de todos los ficheros de todos sus subdirectorios .\nOpciones :\n-b Imprime el tama ño ocupado en disco por todos los bloques del fichero .\n-t SIZE Excluye todos los ficheros más pequeños que SIZE bytes , si es positivo , o más grandes que SIZE bytes , si es negativo , cuando se procesa un directorio .\n-v Imprime el tamaño de todos y cada uno de los ficheros cuando se procesa un directorio .\n-h help\nNota : Todos los tamaños están expresados en bytes .\n");
	else 
	{
		int indiceIni=optind;
		int i=indiceIni;
		int aux=0;
		struct stat sb;
		while(i<argc)
		{
			if (stat(argv[i], &sb) == -1) 
			{
               perror("stat");
               exit(EXIT_FAILURE);
        	}
         	if (S_ISREG(sb.st_mode))
				if (flag_b)
					printf("(F) %s: %7jd\n",argv[i],(sb.st_blocks*512));
				else
					printf("(F) %s: %7jd\n",argv[i],sb.st_size);
			else if (S_ISDIR(sb.st_mode))
			{
		    	if (nftw(argv[i], display_info, 20, flags)== -1) 
				{
					perror("nftw");
					exit(EXIT_FAILURE);
				}
				printf("(D) %s: %d\n", argv[i], tamTotal);		
				tamTotal=0;			
			}
			i++;			
		}
		if (indiceIni==argc)
		{
			if (nftw(".", display_info, 20, flags)== -1) 
				{
					perror("nftw");
					exit(EXIT_FAILURE);
				}
				printf("(D) %s: %d\n", ".", tamTotal);		
				tamTotal=0;			
		}
	}
}















