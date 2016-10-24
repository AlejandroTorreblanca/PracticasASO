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

// Libreadline
#include <readline/readline.h>
#include <readline/history.h>

// Tipos presentes en la estructura `cmd`, campo `type`.
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define EXIT 3
#define MAXARGS 15
#define TAM_PATH 512
#define NUM_INTERNOS 5
#define TAM_BUFF 4096
#define PROFUN 20
const char *COMANDOS_INTERNOS[NUM_INTERNOS]={"pwd","exit","cd","tee", "du"};


// Estructuras
// -----

// La estructura `cmd` se utiliza para almacenar la información que
// servirá al shell para guardar la información necesaria para
// ejecutar las diferentes tipos de órdenes (tuberías, redirecciones,
// etc.)
//
// El formato es el siguiente:
//
//     |----------+--------------+--------------|
//     | (1 byte) | ...          | ...          |
//     |----------+--------------+--------------|
//     | type     | otros campos | otros campos |
//     |----------+--------------+--------------|
//
// Nótese cómo las estructuras `cmd` comparten el primer campo `type`
// para identificar el tipo de la estructura, y luego se obtendrá un
// tipo derivado a través de *casting* forzado de tipo. Se obtiene así
// un polimorfismo básico en C.
struct cmd {
    int type;
};

// Ejecución de un comando con sus parámetros
struct execcmd {
    int type;
    char * argv[MAXARGS];
    char * eargv[MAXARGS];
};

// Ejecución de un comando de redirección
struct redircmd {
    int type;
    struct cmd *cmd;
    char *file;
    char *efile;
    int mode;
    int fd;
};

// Ejecución de un comando de tubería
struct pipecmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

// Lista de órdenes
struct listcmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

// Tarea en segundo plano (background) con `&`.
struct backcmd {
    int type;
    struct cmd *cmd;
};

// Declaración de funciones necesarias
int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parse_cmd(char*);
 
//Función para el comando interno pwd.
void 
run_pwd()
{
	char * path=malloc(TAM_PATH*sizeof(char));
	if (path==NULL)
	{
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	char * cwd=getcwd(path,TAM_PATH);
	if (cwd==NULL)
	{
		perror("getcwd");
		exit(EXIT_FAILURE);
	} 
	fprintf(stderr, "simplesh: pwd: ");
	printf("%s\n",path);
}

//Función para el comando interno exit.
void
run_exit()
{
	exit(EXIT_SUCCESS);	
}

void
run_cd(char** argv)
{
	char* home=getenv("HOME");
	char* ruta=argv[1];
	if (ruta==NULL)
	{
    	if (chdir(home)==-1)
			{
				perror("chdir");
			}
	}
	else
	{
		if (chdir(ruta)==-1)
			{
				perror("chdir");
			}
	}
}

void
run_tee(char** argv)
{
	int argc=0;
	int flag=O_TRUNC;
	int c;
	int ayuda=0;
	while (argc<MAXARGS && argv[argc]!=NULL)
		argc++;
	while(1)
	{
		int option_index=0;
		c=getopt(argc, argv, "ha");
		if(c==-1) 
			break;
		switch(c)
		{
			case 'h':
				ayuda=1;
				break;
			case 'a':
				flag=O_APPEND;
				break;
			default:
				printf ("?? getopt returned character code 0%o ??\n", c);
		}
	}
	if (ayuda)
		printf("Uso: tee [-h] [-a] [FICHERO ]...\nCopia stdin a cada FICHERO y a stdout.\nOpciones:\n-a Añade al final de cada FICHERO\n-h help\n");
	else
	{
		int indiceIni=optind;				
		int i=indiceIni;
		int bytesEscritos;
		int bytesTotales;
		int bytesLeidos;
//Guardará los descriptores de los ficheros abiertos (habrá un -1 si falló).
		int* fd;
		fd=malloc(sizeof(int)*argc);
		if (fd==NULL)
		{
			perror("malloc");
			exit(EXIT_FAILURE);
		}
//Abrir ficheros
		while(i<argc)
		{
			fd[i]=open(argv[i],O_RDWR|O_CREAT|flag,S_IRUSR|S_IWUSR);
			i++;
		}
		char* buff;
		buff=malloc(TAM_BUFF);
		if (buff==NULL)
		{
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		bytesLeidos=read(STDIN_FILENO, buff, TAM_BUFF);
		while(bytesLeidos>0)
		{
			i=indiceIni;
//Escribir en todos los ficheros (si fd obtenido=!-1, es decir, abiertos correctamente)
			while (i<argc)
			{
				if(fd[i]!=-1)
				{
					bytesEscritos=0;
					bytesTotales=0;
					while(bytesTotales<bytesLeidos)
					{
						bytesEscritos=write(fd[i], buff+bytesTotales,bytesLeidos-bytesTotales);
						bytesTotales+=bytesEscritos;
						if (bytesEscritos==-1)
						{
							perror("write");
							exit(EXIT_FAILURE);
						}
					}
				}
				i++;
			}
//Escribir en salida estándar
			bytesEscritos=write(STDOUT_FILENO, buff,bytesLeidos);
			bytesTotales=bytesEscritos;
			if (bytesEscritos==-1)
			{
				perror("write");
				exit(EXIT_FAILURE);
			}
			while(bytesTotales<bytesLeidos)
			{
				if (bytesEscritos==-1)
				{
					perror("write");
					exit(EXIT_FAILURE);
				}
				bytesTotales+=bytesEscritos;
				bytesEscritos=write(STDOUT_FILENO, buff+bytesTotales,bytesLeidos-bytesTotales);
			}
//Leer la siguiente ristra de la entrada estándar
			bytesLeidos=read(STDIN_FILENO, buff, TAM_BUFF);
		}
		if (bytesLeidos==-1)				
		{
			perror("read");
			exit(EXIT_FAILURE);
		}
//Obligar a que los ficheros sean escritos en disco
		i=indiceIni;
		while (i<argc)
		{
			if(fd[i]!=-1)
			{
				if (fsync(fd[i])==-1)
				{
					perror("fsync");
					exit(EXIT_FAILURE);
				}
			}
			i++;
		}
// Cerrar ficheros
		i=indiceIni;
		while (i<argc)
		{
			if(fd[i]!=-1)
			{
				if (close(fd[i])==-1)
				{
					perror("close");
					exit(EXIT_FAILURE);
				}
			}
			i++;
		}
							///OPCIONAL:

		char* home=getenv("HOME");	
		strcat(home,"/.tee.log");
		int ficherosEscritos=0;
		int k=indiceIni;
		while(k<argc)
		{
			if (fd[k]!=-1)
				ficherosEscritos++;
			k++;
		}
		fd[0]=open(home,O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);	
			if (fd[0]==0)
			{
				perror("open");
				exit(EXIT_FAILURE);
			}
		time_t rawtime, errortime;
	   	struct tm *info;
	   	char buffer[256];
		char str[32];

	   	errortime=time( &rawtime );
		if (errortime==-1)
		{
			perror("time");
			exit(EXIT_FAILURE);
		}
	   	info = localtime( &rawtime );
	
		strftime(buffer,80,"%Y-%m-%d  %X:", info);
	  	
		int pid =(int)getpid();
		int euid=(int)getuid();

		strcat(buffer, "PID ");
   		sprintf(str,"%d", pid);
		strcat(buffer,str);
		strcat(buffer, ":EUID ");
		sprintf(str,"%d", euid);
		strcat(buffer,str);
		strcat(buffer,":");
		sprintf(str,"%d", bytesTotales);
		strcat(buffer,str);
		strcat(buffer,":");
		sprintf(str,"%d \n", ficherosEscritos);
		strcat(buffer,str);
		bytesEscritos=write(fd[0], buffer,strlen(buffer));
		if (bytesEscritos==-1)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}
		if (close(fd[0])==-1)
		{
			perror("close");
			exit(EXIT_FAILURE);
		}

		free(buff);
		free(fd);
	}		
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


//Comprobar si un comando es una orden interna.
int
es_interno(char* comando)
{
	int ret=0;
	int i=0;
	while(i<NUM_INTERNOS && ret==0)
	{
		if (strcmp(comando,COMANDOS_INTERNOS[i])==0)
			ret=1;
		i++;
	}
	return ret;
}

//Ejecutar un comando interno
void
run_interno(char** argv)
{
	int i=0;
	while(strcmp(argv[0],COMANDOS_INTERNOS[i])!=0)
		i++;
	switch (i) 
	{
		case 0: run_pwd();
			break;
		case 1: run_exit();
			break;

		case 2: run_cd(argv);
			break;
		case 3: run_tee(argv);
			break;
		case 4: run_du(argv);
			break;
		default: break;
	}
}

//Recorrido del árbol de la línea parseada.
int recorridoProfundidad(struct cmd* cmd, int res)
{
	static int numCd;
	if (res)
		numCd=0;
	struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if(cmd == 0)
        exit(0);

    switch(cmd->type)
    {
    default:
        panic("recorrido");

    case EXEC:
        ecmd = (struct execcmd*)cmd;  
		if (strcmp(ecmd->argv[0],COMANDOS_INTERNOS[1])==0)
			return 1;
		else if (strcmp(ecmd->argv[0],COMANDOS_INTERNOS[2])==0)
			run_cd(ecmd->argv);
        break;

    case REDIR:
        rcmd = (struct redircmd*)cmd;
       	return recorridoProfundidad(rcmd->cmd, 0);

    case LIST:
        lcmd = (struct listcmd*)cmd;
		return (recorridoProfundidad(lcmd->left, 0) || recorridoProfundidad(lcmd->right, 0));

    case PIPE:
        pcmd = (struct pipecmd*)cmd; 
		return (recorridoProfundidad(pcmd->left, 0) || recorridoProfundidad(pcmd->right, 0));  

    case BACK:
        bcmd = (struct backcmd*)cmd;
		return recorridoProfundidad(bcmd->cmd, 0);
    }
	return 0;
}

// Ejecuta un `cmd`. Nunca retorna, ya que siempre se ejecuta en un
// hijo lanzado con `fork()`.
void
run_cmd(struct cmd *cmd)
{
    int p[2];
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if(cmd == 0)
        exit(0);

    switch(cmd->type)
    {
    default:
        panic("run_cmd");

        // Ejecución de una única orden.
    case EXEC:
        ecmd = (struct execcmd*)cmd;
        if (ecmd->argv[0] == 0)
            exit(0);
		else if(es_interno(ecmd->argv[0])==1){
			run_interno(ecmd->argv);
			exit(EXIT_SUCCESS);
		}
		else 
		{
       		execvp(ecmd->argv[0], ecmd->argv);
        	// Si se llega aquí algo falló
        	fprintf(stderr, "exec %s failed\n", ecmd->argv[0]);
        	exit (1);
		}
        break;

    case REDIR:
        rcmd = (struct redircmd*)cmd;
        close(rcmd->fd);
        if (open(rcmd->file, rcmd->mode, S_IRUSR|S_IWUSR) < 0)
        {
            fprintf(stderr, "open %s failed\n", rcmd->file);
            exit(1);
        }
        run_cmd(rcmd->cmd);
        break;

    case LIST:
        lcmd = (struct listcmd*)cmd;
        if (fork1() == 0)
            run_cmd(lcmd->left);
		wait(NULL);
//Si es un exit o cmd lo tengo que hacer yo también para un correcto funcionamiento
		if (lcmd->left->type==EXEC)
		{
			ecmd=(struct execcmd*) lcmd->left;
			if (strcmp(ecmd->argv[0],COMANDOS_INTERNOS[1])==0)
				run_exit();
			else if (strcmp(ecmd->argv[0],COMANDOS_INTERNOS[2])==0)
				run_cd(ecmd->argv);
		}
        run_cmd(lcmd->right);
        break;

    case PIPE:
        pcmd = (struct pipecmd*)cmd;
        if (pipe(p) < 0)
            panic("pipe");

        // Ejecución del hijo de la izquierda
        if (fork1() == 0)
        {
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            run_cmd(pcmd->left);
        }

        // Ejecución del hijo de la derecha
        if (fork1() == 0)
        {
            close(0);
            dup(p[0]);
            close(p[0]);
            close(p[1]);
            run_cmd(pcmd->right);
        }
        close(p[0]);
        close(p[1]);

        // Esperar a ambos hijos
        wait(NULL);
        wait(NULL);
        break;

    case BACK:
        bcmd = (struct backcmd*)cmd;
        if (fork1() == 0)
            run_cmd(bcmd->cmd);
        break;
    }

    // Salida normal, código 0.
    exit(0);
}

// Muestra un *prompt* y lee lo que el usuario escribe usando la
// librería readline. Ésta permite almacenar en el historial, utilizar
// las flechas para acceder a las órdenes previas, búsquedas de
// órdenes, etc.
char*
getcmd()
{
    char *buf;
    int retval = 0;
    uid_t uid=getuid();
	struct passwd* pw=getpwuid(uid);
	if (pw==NULL){
		perror("getpwuid");
		exit(EXIT_FAILURE);
	} 
	char * path=malloc(TAM_PATH*sizeof(char));
	if (path==NULL){
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	char * cwd=getcwd(path,TAM_PATH);
	if (cwd==NULL){
		perror("getcwd");
		exit(EXIT_FAILURE);
	} 
	char * dir=basename(path);
	char * prompt=malloc(TAM_PATH*sizeof(char));
	if (prompt==NULL){
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	int cx=snprintf(prompt, TAM_PATH, "$ %s@%s ", pw->pw_name, dir);
	if (cx<0 || cx>=TAM_PATH) {
		printf("prompt demasiado largo");
	}
    // Lee la entrada del usuario
    buf = readline (prompt);
	
    // Si el usuario ha escrito algo, almacenarlo en la historia.
    if(buf)
        add_history (buf);
	free(path);
	free(prompt);
    return buf;
}

// Función `main()`.
// ----

int
main(void)
{
    char* buf;
    // Bucle de lectura y ejecución de órdenes.
    while (NULL != (buf = getcmd()))
    {
		struct cmd* cmd=parse_cmd(buf);
        // Crear siempre un hijo para ejecutar el comando leído
        if(fork1() == 0)
            run_cmd(cmd);
		wait(NULL);
		if (cmd!=NULL)
			if (recorridoProfundidad(cmd, 1))
				run_exit();
        free ((void*)buf);
    }
    return 0;
}

void
panic(char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(-1);
}

// Como `fork()` salvo que muestra un mensaje de error si no se puede
// crear el hijo.
int
fork1(void)
{
    int pid;
    pid = fork();
    if(pid == -1)
        panic("fork");
    return pid;
}

// Constructores de las estructuras `cmd`.
// ----

// Construye una estructura `EXEC`.
struct cmd*
execcmd(void)
{
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;
    return (struct cmd*)cmd;
}

// Construye una estructura de redirección.
struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    return (struct cmd*)cmd;
}

// Construye una estructura de tubería (*pipe*).
struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*)cmd;
}

// Construye una estructura de lista de órdenes.
struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
    struct listcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*)cmd;
}

// Construye una estructura de ejecución que incluye una ejecución en
// segundo plano.
struct cmd*
backcmd(struct cmd *subcmd)
{
    struct backcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (struct cmd*)cmd;
}

// Parsing
// ----

const char whitespace[] = " \t\r\n\v";
const char symbols[] = "<|>&;()";

// Obtiene un *token* de la cadena de entrada `ps`, y hace que `q` apunte a
// él (si no es `NULL`).
int
gettoken(char **ps, char *end_of_str, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while (s < end_of_str && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    ret = *s;
    switch (*s)
    {
    case 0:
        break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
        s++;
        break;
    case '>':
        s++;
        if (*s == '>')
        {
            ret = '+';
            s++;
        }
        break;

    default:
        // El caso por defecto (no hay caracteres especiales) es el de un
        // argumento de programa. Se retorna el valor `'a'`, `q` apunta al
        // mismo (si no era `NULL`), y `ps` se avanza hasta que salta todos
        // los espacios **después** del argumento. `eq` se hace apuntar a
        // donde termina el argumento. Así, si `ret` es `'a'`:
        //
        //     |-----------+---+---+---+---+---+---+---+---+---+-----------|
        //     | (espacio) | a | r | g | u | m | e | n | t | o | (espacio) |
        //     |-----------+---+---+---+---+---+---+---+---+---+-----------|
        //                   ^                                   ^
        //                   |q                                  |eq
        //
        ret = 'a';
        while (s < end_of_str && !strchr(whitespace, *s) && !strchr(symbols, *s))
            s++;
        break;
    }

    // Apuntar `eq` (si no es `NULL`) al final del argumento.
    if (eq)
        *eq = s;

    // Y finalmente saltar los espacios en blanco y actualizar `ps`.
    while(s < end_of_str && strchr(whitespace, *s))
        s++;
    *ps = s;

    return ret;
}

// La función `peek()` recibe un puntero a una cadena, `ps`, y un final de
// cadena, `end_of_str`, y un conjunto de tokens (`toks`). El puntero
// pasado, `ps`, es llevado hasta el primer carácter que no es un espacio y
// posicionado ahí. La función retorna distinto de `NULL` si encuentra el
// conjunto de caracteres pasado en `toks` justo después de los posibles
// espacios.
int
peek(char **ps, char *end_of_str, char *toks)
{
    char *s;

    s = *ps;
    while(s < end_of_str && strchr(whitespace, *s))
        s++;
    *ps = s;

    return *s && strchr(toks, *s);
}

// Definiciones adelantadas de funciones.
struct cmd *parse_line(char**, char*);
struct cmd *parse_pipe(char**, char*);
struct cmd *parse_exec(char**, char*);
struct cmd *nulterminate(struct cmd*);

// Función principal que hace el *parsing* de una línea de órdenes dada por
// el usuario. Llama a la función `parse_line()` para obtener la estructura
// `cmd`.
struct cmd*
parse_cmd(char *s)
{
    char *end_of_str;
    struct cmd *cmd;

    end_of_str = s + strlen(s);
    cmd = parse_line(&s, end_of_str);

    peek(&s, end_of_str, "");
    if (s != end_of_str)
    {
        fprintf(stderr, "restante: %s\n", s);
        panic("syntax");
    }

    // Termina en `'\0'` todas las cadenas de caracteres de `cmd`.
    nulterminate(cmd);

    return cmd;
}

// *Parsing* de una línea. Se comprueba primero si la línea contiene alguna
// tubería. Si no, puede ser un comando en ejecución con posibles
// redirecciones o un bloque. A continuación puede especificarse que se
// ejecuta en segundo plano (con `&`) o simplemente una lista de órdenes
// (con `;`).
struct cmd*
parse_line(char **ps, char *end_of_str)
{
    struct cmd *cmd;

    cmd = parse_pipe(ps, end_of_str);
    while (peek(ps, end_of_str, "&"))
    {
        gettoken(ps, end_of_str, 0, 0);
        cmd = backcmd(cmd);
    }

    if (peek(ps, end_of_str, ";"))
    {
        gettoken(ps, end_of_str, 0, 0);
        cmd = listcmd(cmd, parse_line(ps, end_of_str));
    }

    return cmd;
}

// *Parsing* de una posible tubería con un número de órdenes.
// `parse_exec()` comprobará la orden, y si al volver el siguiente *token*
// es un `'|'`, significa que se puede ir construyendo una tubería.
struct cmd*
parse_pipe(char **ps, char *end_of_str)
{
    struct cmd *cmd;

    cmd = parse_exec(ps, end_of_str);
    if (peek(ps, end_of_str, "|"))
    {
        gettoken(ps, end_of_str, 0, 0);
        cmd = pipecmd(cmd, parse_pipe(ps, end_of_str));
    }

    return cmd;
}


// Construye los comandos de redirección si encuentra alguno de los
// caracteres de redirección.
struct cmd*
parse_redirs(struct cmd *cmd, char **ps, char *end_of_str)
{
    int tok;
    char *q, *eq;

    // Si lo siguiente que hay a continuación es una redirección...
    while (peek(ps, end_of_str, "<>"))
    {
        // La elimina de la entrada
        tok = gettoken(ps, end_of_str, 0, 0);

        // Si es un argumento, será el nombre del fichero de la
        // redirección. `q` y `eq` tienen su posición.
        if (gettoken(ps, end_of_str, &q, &eq) != 'a')
            panic("missing file for redirection");

        switch(tok)
        {
        case '<':
            cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
            break;
        case '>':
            cmd = redircmd(cmd, q, eq, O_RDWR|O_CREAT|O_TRUNC, 1);
            break;
        case '+':  // >>
            cmd = redircmd(cmd, q, eq, O_RDWR|O_CREAT|O_APPEND, 1);
            break;
        }
    }

    return cmd;
}

// *Parsing* de un bloque de órdenes delimitadas por paréntesis.
struct cmd*
parse_block(char **ps, char *end_of_str)
{
    struct cmd *cmd;

    // Esperar e ignorar el paréntesis
    if (!peek(ps, end_of_str, "("))
        panic("parse_block");
    gettoken(ps, end_of_str, 0, 0);

    // Parse de toda la línea hsta el paréntesis de cierre
    cmd = parse_line(ps, end_of_str);

    // Elimina el paréntesis de cierre
    if (!peek(ps, end_of_str, ")"))
        panic("syntax - missing )");
    gettoken(ps, end_of_str, 0, 0);

    // ¿Posibles redirecciones?
    cmd = parse_redirs(cmd, ps, end_of_str);

    return cmd;
}

// Hace en *parsing* de una orden, a no ser que la expresión comience por
// un paréntesis. En ese caso, se inicia un grupo de órdenes para ejecutar
// las órdenes de dentro del paréntesis (llamando a `parse_block()`).
struct cmd*
parse_exec(char **ps, char *end_of_str)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    // ¿Inicio de un bloque?
    if (peek(ps, end_of_str, "("))
        return parse_block(ps, end_of_str);

    // Si no, lo primero que hay una línea siempre es una orden. Se
    // construye el `cmd` usando la estructura `execcmd`.
    ret = execcmd();
    cmd = (struct execcmd*)ret;

    // Bucle para separar los argumentos de las posibles redirecciones.
    argc = 0;
    ret = parse_redirs(ret, ps, end_of_str);
    while (!peek(ps, end_of_str, "|)&;"))
    {
        if ((tok=gettoken(ps, end_of_str, &q, &eq)) == 0)
            break;

        // Aquí tiene que reconocerse un argumento, ya que el bucle para
        // cuando hay un separador
        if (tok != 'a')
            panic("syntax");

        // Apuntar el siguiente argumento reconocido. El primero será la
        // orden a ejecutar.
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if (argc >= MAXARGS)
            panic("too many args");

        // Y de nuevo apuntar posibles redirecciones
        ret = parse_redirs(ret, ps, end_of_str);
    }

    // Finalizar las líneas de órdenes
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;

    return ret;
}

// Termina en NUL todas las cadenas de `cmd`.
struct cmd*
nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if(cmd == 0)
        return 0;

    switch(cmd->type)
    {
    case EXEC:
        ecmd = (struct execcmd*)cmd;
        for(i=0; ecmd->argv[i]; i++)
            *ecmd->eargv[i] = 0;
        break;

    case REDIR:
        rcmd = (struct redircmd*)cmd;
        nulterminate(rcmd->cmd);
        *rcmd->efile = 0;
        break;

    case PIPE:
        pcmd = (struct pipecmd*)cmd;
        nulterminate(pcmd->left);
        nulterminate(pcmd->right);
        break;

    case LIST:
        lcmd = (struct listcmd*)cmd;
        nulterminate(lcmd->left);
        nulterminate(lcmd->right);
        break;

    case BACK:
        bcmd = (struct backcmd*)cmd;
        nulterminate(bcmd->cmd);
        break;
    }

    return cmd;
}

/*
 * Local variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 75
 * eval: (auto-fill-mode t)
 * End:
 */
