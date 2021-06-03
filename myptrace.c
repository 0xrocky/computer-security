#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>	
#include <sys/syscall.h>
#include <sys/reg.h>

#define indEBX 0
#define indECX 1
#define indEDX 2
#define indESI 3
#define indEDI 4
#define indEAX 5
#define SIZE_MAX 1024

/* Procedura che preleva una stringa dal processo pid alla locazione addr, e la memorizza in str con dimensione pari a len  */
void getString( pid_t pid, long addr, char *str, int len ) {
	char *laddr;
	const int longSize = sizeof( long );
	/* Uso la struttura union perché PEEKDATA restituisce una word (long) che in realtà sarà una word della stringa-parametro: quindi ho un unico valore che assume tipo diverso; i campi condividono lo stesso spazio di memoria (dimensione determinata dal più grande) */
	union peekdata {
		long val;		/* valore restituito dalla ptrace peekdata */
		char chars[ longSize ];	/* corrispondente array di longSize chars, usato come area di memoria tampone */
	} data;
	int i = 0;
	int j = len / longSize;		/* numero word contigue */
	laddr = str;
	while( i < j ) {
	/* con PEEKTEXT ottengo stesso risultato perché {data,text}section (dati e codice) condividono lo stesso address space in Linux */
		data.val = ptrace( PTRACE_PEEKDATA, pid, ( void * ) addr + i*4, 0 );
		memcpy( laddr, data.chars, longSize );	/* ricompongo la stringa: copio longSize bytes da data.chars a laddr */
		i++;
		laddr += longSize;			/* mi sposto di longSize, incremento puntatore */
	}
	/* Copio eventuali bytes rimanenti */
	j = len % longSize;
	if( j != 0 ) {
		data.val = ptrace( PTRACE_PEEKDATA, pid, ( void * ) addr + i*4, 0 );
		memcpy( laddr, data.chars, j );
	}
	/* Eventuale procedura di trimming dei line feed in spazi, per avere tutto l'output su una riga */
	/* for( i = 0; i < len; i++ ) {
		if( str[ i ] <= 31 ) {
			if ( str[ i ] == '\n' )
				str[ i ] = ' ';
			else if( str[ i ] == '\t' || str[ i ] == 'r' );
			else str[ i ] = '\0';
		}
	} */
	str[ len-1 ] = '\0';	/* Solitamente l'ultimo carattere è un line feed: lo sostituisco con un null character */
}

int main( int argc, char **argv ) {
	
	pid_t pid = fork();			/* child process id: con fork creo nuovo processo (con id = pid) copia del corrente (parent) */
	int status = 0;				/* stato del child, serve a capire se è terminato o bloccato su una syscall */
	int enter = 0;				/* variabile che distingue se sono all'entrata/uscita di una syscall */
	long syscall = 0;			/* numero della syscall intercettata */
	long regs[ 6 ];				/* registri general purpose */
	long ptr, ptrTemp;
	char *str = NULL;
	
	/* Con un errore di fork o se non ho argomenti da linea di comando, semplicemente esco dal programma */
	if( pid == -1 || !argc || !argv[ 1 ] ) {
		perror("Error\n");
		return( -1 );
	}
	/* Child process: nel processo figlio la fork ritorna processi id = zero */
	else if( pid == 0 ) {
		ptrace( PTRACE_TRACEME, 0, 0, 0 );
		argv[ argc ] = 0;
		/* child esegue il programma, su cui ora parent potrà fare trace */
		execvp( argv[ 1 ], &argv[ 1 ] );
	}
	/* Parent process: nel processo padre la fork ritorna il pid del figlio */
	else {
		wait( &status );				/* In attesa sulla exec */
		while( 1 ) {
			ptrace( PTRACE_SYSCALL, pid, 0, 0 );	/* Traccia la syscall del child pid */
			wait( &status );			/* In attesa della notifica della syscall del pid */
			if ( WIFEXITED( status ) ) break;	/* WIFEXITED: se il child pid è terminato esco dal ciclo */
			/* Recupero il valore dei registri general purpose: orig.eax contiene l'id della syscall, eax il valore di ritorno */
			syscall = ptrace( PTRACE_PEEKUSER, pid, ORIG_EAX*4, 0 );
			regs[ indEBX ] = ptrace( PTRACE_PEEKUSER, pid, EBX*4, 0 );
			regs[ indECX ] = ptrace( PTRACE_PEEKUSER, pid, ECX*4, 0 );
			regs[ indEDX ] = ptrace( PTRACE_PEEKUSER, pid, EDX*4, 0 );
			regs[ indESI ] = ptrace( PTRACE_PEEKUSER, pid, ESI*4, 0 );			
			regs[ indEDI ] = ptrace( PTRACE_PEEKUSER, pid, EDI*4, 0 );
			regs[ indEAX ] = ptrace( PTRACE_PEEKUSER, pid, EAX*4, 0 );
			/* Sono all'uscita della syscall */			
			if( enter ) {
				printf( "syscall_#%d(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx) = 0x%lx\n", syscall, regs[ indEBX ], regs[ indECX ], regs[ indEDX ], regs[ indESI ], regs[ indEDI ], regs[ indEAX ] );
				enter = 0;
			}
			else {	/* Sono all'entrata della syscall */
				switch( syscall ) {
					case SYS_exit:	/* 1: exit */
						printf( "syscall_exit(%ld)\n", regs[ indEBX ] );
						break;
					case SYS_read: /* 3: read */
						str = ( char * ) malloc( (regs[ indEDX ] ) * sizeof( char ) );
						getString( pid, regs[ indECX ], str, regs[ indEDX ] );
						printf( "syscall_read(%ld, \"%s\", 0x%lx)\n", regs[ indEBX ], str, regs[ indEDX ] );
						if( str != NULL ) free( str );
						break;
					case SYS_write: /* 4: write */
						str = ( char * ) malloc( (regs[ indEDX ] ) * sizeof( char ) );
						getString( pid, regs[ indECX ], str, regs[ indEDX ] );
						printf( "syscall_write(%ld, \"%s\", 0x%lx)\n", regs[ indEBX ], str, regs[ indEDX ] );
						if( str != NULL ) free( str );
						break;
					case SYS_open: /* 5: open */
						str = ( char * ) malloc( ( SIZE_MAX ) * sizeof( char ) );
						getString( pid, regs[ indEBX ], str, SIZE_MAX );
						printf( "syscall_open(\"%s\", 0x%lx, 0x%lx)\n", str, regs[ indECX ], regs[ indEDX ] );
						if ( str != NULL ) free( str );
						break;
					case SYS_execve: /* 11: execve */
						/* Setto opzione di exec per non deliverare SIGTRAP extra */
						ptrace( PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEEXEC );
						/* Estraggo filename del programma da ebx, e stampo */
						if( regs[ indEBX ] ) {
							ptr = regs[ indEBX ];
							str = ( char * ) malloc( ( SIZE_MAX ) * sizeof( char ) );
							getString( pid, ptr, str, SIZE_MAX );
							printf( "syscall_execve(\"%s\", ", str );
							if ( str != NULL ) free( str );
						
							/* Estraggo dalla execve gli argomenti del programma, da ecx, e stampo */
							for ( ptrTemp = regs[ indECX ]; ptrTemp; ptrTemp += sizeof( long ) ) {
								ptr = ptrTemp;
								/* char *argv[]: doppia indirezione */
		               					ptr = ptrace( PTRACE_PEEKDATA, pid, ( void * ) ptr, 0 );
		               					if ( !ptr ) break;
		               					str = ( char * ) malloc( ( SIZE_MAX ) * sizeof( char ) );
								getString( pid, ptr, str, SIZE_MAX );
								printf( "\"%s\"", str );
								if ( str != NULL ) free( str );
							}
							printf( ", " );
							
							/* Estraggo dalla execve le variabili d'ambiente, da edx, e stampo */
							for ( ptrTemp = regs[ indEDX ]; ptrTemp; ptrTemp += sizeof( long ) ) {
								ptr = ptrTemp;
								/* char *envp[]: doppia indirezione */
								ptr = ptrace( PTRACE_PEEKDATA, pid, ( void * ) ptr, 0 );
								if ( !ptr ) break;
								str = ( char * ) malloc( ( SIZE_MAX ) * sizeof( char ) );
								getString( pid, ptr, str, SIZE_MAX );
								printf( "\"%s\"", str );
								if ( str != NULL ) free( str );
							}
							printf( ")\n" );
						}
						break;
				}
				enter = 1;
			}
		}
	}
	return 0;
}
