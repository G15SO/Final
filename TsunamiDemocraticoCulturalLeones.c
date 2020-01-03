#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>


pthread_mutex_t mutexSolicitudes, mutexCultural, mutexLogs;
pthread_cond_t condCoordinador, condInicioActi;
int contadorSolicitudes, contadorCultural, solicitudesEncoladas, fin, estadoCultural, maxSolicitudes;
FILE *logFile;
/*
*Structura que emula una solicitud a la aplicacion.
*ID: corresponde con el ID que tendra en la aplicacion.
*atendido: vale 0 si no esta atendido, 1 si si lo esta siendo o 2 si ya ha sido atendido CORRECTAMENTE, 3 si se encuentra en una actividad cultural o 4 si ha sido atendido erroneamente.
*tipo: vale 1 si es de tipo invitacion o 2 si es de tipo qr.
*sitio: vale 0 si en esa posicion de la cola no hay alguien sentado o 1 si si lo hay.
*posicion: corresponde con la posicion que ocupa en la cola.
*/
struct Solicitudes{
	int ID;
	int atendido;
	int tipo;
	int sitio;
	int posicion;
};

/*
*Structura que eumula un atendedor en la aplicacion.
*ID: corresponde con el ID que tendra en la apliacacion.
*tipo: corresponde con el tipo de de atendedor que es, 1 atendedor invitaciones, 2 atendedor QR, 3 atendedor PRO.
*solicitudesAtendidas: corresponde con el numero de solicitudes que han atendido para tomar cafe.
*/
struct Atendedores{
	int ID;
	int tipo;
	int solicitudesAtendidas;
};

//Cola de solicitudes
struct Solicitudes *colaSolicitudes;
//Cola de solicitudes en la actividad cultural
struct Solicitudes colaCultural[4];
//Lista con los atendedores
struct Atendedores *atendedores;

//Declaracion de las funciones utilizadas.
int aleatorios(int min, int max);
void manSolicitud(int sig);
void manTerminacion(int sig);
void *accionesAtendedor(void *arg);
void *accionesCoordinadorSocial(void *arg);
void *accionesSolicitud(void *arg);
int encuentraSitio();
void manFin(int sig);
void manMenu(int sig);
int buscarSolicitud(int tipoAtendedor);
int buscaMasAntigua(int *solicitudes, int contador);
void creaIdentificador(char *id, int num);
void writeLogMessage(char *id, char *msg);

//Funcion principal
int main(int argc, char *argv[]){
	int i, numAtendedores;
	struct sigaction ss = {0}, ss2 = {0}, ss3 = {0};
	srand(time(NULL));
	//Tratado de los parametros de ejecucion
	if(argc == 1){
		maxSolicitudes = 15;
		numAtendedores = 4;
	}else if(argc == 3){
		maxSolicitudes =(int) atoi(argv[1]);
		numAtendedores =(int) atoi(argv[2])+3;
		if(maxSolicitudes < 1){
			printf("El numero de solicitudes maximas debe de ser mayor a 0\n");
			exit(-1);
		}
		if(numAtendedores-3 < 0){
			printf("El numero de atendedores PRO debe de ser mayor o igual a 0\n");
			exit(-1);
		}
	}else{
		printf("Parametros erroneos, deben de ser 0 o 2, introduciendo primero el numero de solicitudes y luego el de atendedores PRO\n");
		exit(-1);
	}
	//Inicializacion de las colas
	colaSolicitudes = malloc(maxSolicitudes*sizeof(struct Solicitudes));
	atendedores = malloc(numAtendedores*sizeof(struct Atendedores));
	//Inicializacion de mutex y variables condicion
	pthread_mutex_init(&mutexSolicitudes, NULL);
	pthread_mutex_init(&mutexCultural, NULL);
	pthread_mutex_init(&mutexLogs, NULL);
	pthread_cond_init(&condCoordinador, NULL);
	pthread_cond_init(&condInicioActi, NULL);
	/*Inicializacion de las posiciones 0 de las colas en la que estara reflejado
	* el numero de atendedores o de solicitudes que tiene el programa.
	*/
	atendedores[0].ID = 1;
	atendedores[0].tipo = 1;
	atendedores[0].solicitudesAtendidas = numAtendedores - 3;
	//Creacion e inicializacion de los atendedores
	pthread_t atendedorIn, atendedorQR, atendedorPro, coordinador;
	atendedores[1].tipo = 1;
	atendedores[1].ID = 1;
	atendedores[1].solicitudesAtendidas = 0;
	atendedores[2].tipo = 2;
	atendedores[2].ID = 2;
	atendedores[2].solicitudesAtendidas = 0;
	for(i = 3; i < numAtendedores; i++){
		atendedores[i].tipo = 3;
		atendedores[i].ID = i;
		atendedores[i].solicitudesAtendidas = 0;
	}
	//inicializacion cola solicitudes
	for(i = 0; i<maxSolicitudes; i++){
		colaSolicitudes[i].ID = 0;
		colaSolicitudes[i].tipo = 0;
		colaSolicitudes[i].atendido = 0;
		colaSolicitudes[i].sitio = 0;
		colaSolicitudes[i].posicion = 0;
	}
	//inicializacion cola cultural
	for(i = 0; i<4; i++){
		colaCultural[i].ID = 0;
		colaCultural[i].tipo = 0;
		colaCultural[i].atendido = 0;
		colaCultural[i].sitio = 0;
		colaCultural[i].posicion = 0;
	}
	//Inicializacion variables globales
	contadorSolicitudes = 0;
	solicitudesEncoladas = 0;
	contadorCultural = 0;
	fin = 0;
	estadoCultural = 0;
	//tratamiento de SIGUSR1 para crear un nuevo hilo de tipo invitacion
	ss.sa_handler = manSolicitud;
	if (sigaction(SIGUSR1, &ss, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	//tratamiento de SIGUSR2 para crear un nuevo hilo de tipo qr
	if (sigaction(SIGUSR2, &ss, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	//tratamiento de SIGINT para finalizar el programa matando a todos los hilos
	ss2.sa_handler = manTerminacion;
	if (sigaction(SIGINT, &ss2, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	ss3.sa_handler = manMenu;
	if (sigaction(SIGPIPE, &ss3, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	//creacion de los hilos atendedores y coordinador
	pthread_create(&atendedorIn, NULL, accionesAtendedor, (void *)&atendedores[1]);
	pthread_create(&atendedorQR, NULL, accionesAtendedor, (void *)&atendedores[2]);
	for(i = 3; i < numAtendedores; i++){
		pthread_create(&atendedorPro, NULL, accionesAtendedor, (void *)&atendedores[i]);
	}
	pthread_create(&coordinador, NULL, accionesCoordinadorSocial, NULL);
	printf("\x1b[33m->PARA DESPLEGAR EL MENU DE ASIGNACIONES DINAMICA, ENVIE LA SEÑAL SIGPIPE<- \x1b[37m\n");
	//espera infinita a señales mientras no se termine la aplicacion
	while (1){
		pause();
	}
}

/*
*Funcion que genera un numero aleatorio en el rango min = minimo, max = maximo
*/
int aleatorios(int min, int max){
	return rand() % (max - min + 1) + min;
}

/*
*Funcion manejadora de SIGUSR1 y SIGURS2 que crea una gestiona las nuevas solicitudes en el programa,
*si hay espacion en la cola crea una un nuevo hilo para esa solicitud, si no hay espacio en la cola ignora la señal.
*/
void manSolicitud(int sig){
	pthread_t t1;
	int posicion = 0, signal = -1;
	if (sig == SIGUSR1){
		signal = 1;
	}
	if (sig == SIGUSR2){
		signal = 2;
	}
	pthread_mutex_lock(&mutexSolicitudes);
	if (solicitudesEncoladas < maxSolicitudes){
		contadorSolicitudes++;
		solicitudesEncoladas++;
		posicion = encuentraSitio();
		if (posicion == -1){
			exit(-1);
		}
		colaSolicitudes[posicion].ID = contadorSolicitudes;
		colaSolicitudes[posicion].sitio = 1;
		colaSolicitudes[posicion].atendido = 0;
		colaSolicitudes[posicion].tipo = signal;
		colaSolicitudes[posicion].posicion = posicion;
		pthread_create(&t1, NULL, accionesSolicitud, (void *)&colaSolicitudes[posicion].posicion);
	}else{
		printf("APP: Señal ignorada por falta de espacio.\n");
	}
	pthread_mutex_unlock(&mutexSolicitudes);
}

/*
*Funcion que encuentra un sitio disponible en la cola de solicitudes, devuelve una posicion libre.
*/
int encuentraSitio(){
	int i = 0, encontrado = 0, posicion = -1;
	while ((i < maxSolicitudes) && (encontrado == 0)){
		if (colaSolicitudes[i].sitio == 0){
			encontrado = 1;
			posicion = i;
		}
		i++;
	}
	return posicion;
}

/*
*Funcion que emula las acciones de una solicitud en el programa.
*La solicitud encolada es atendida por un atendedor y si es atendida correctamente puede o no solicitar unirse a una actividad,
*si decide unirse realizara la actividad y finalizara.
*/
void *accionesSolicitud(void *arg){
	//Continua vale 0 si el hilo puede continuar o 1 si no puede.
	int pos = *(int *)arg, continua, aleatorio, actividad = 0, ID, aux;
	char identificador[30], mensaje[200];
	ID = colaSolicitudes[pos].ID;
	pos = colaSolicitudes[pos].posicion;
	//Creamos mensajes de los logs
	strcpy(identificador, "Solicitud_");
	creaIdentificador(identificador, ID);
	if(colaSolicitudes[pos].tipo == 1){
		strcpy(mensaje, "de tipo invitación ha sido recibida.");
	}else{
		strcpy(mensaje, "de tipo QR ha sido recibida.");
	}
	printf("%s: \x1b[32m%s \x1b[37m\n",identificador, mensaje);
	//Cogemos el mutex de los logs y escribimos en el fichero
	pthread_mutex_lock(&mutexLogs);
	writeLogMessage(identificador, mensaje);
	pthread_mutex_unlock(&mutexLogs);
	//Dormimos 4 segundos antes de comprobar si hemos sido atendidos
	sleep(4);
	//Si no ha sido atendido entra en el if
	if (colaSolicitudes[pos].atendido == 0){
		//Mientras no haya sido atendido realizara el bucle
		do{
			//Comprueba si ha debe descartarse
			if (colaSolicitudes[pos].tipo == 1){
				aleatorio = aleatorios(1, 100);
				if (aleatorio <= 10){
					strcpy(mensaje, "abandona la cola por cansarse de esperar.");
					continua = 1;
				}
			}else{
				aleatorio = aleatorios(1, 100);
				if (aleatorio <= 30){
					strcpy(mensaje, "abandona la cola por no considerarse muy fiable.");
					continua = 1;
				}
			}
			aleatorio = aleatorios(1, 100);
			if (aleatorio <= 15){
				strcpy(mensaje, "es eliminada de la cola por decision de la aplicacion.");
				continua = 1;
			}
			//Si debe descartarse abandona la cola, sino duerme 4 segundos y vuelva a comprobar si ha sido atendido
			if (continua == 1){
				printf("%s: \x1b[31m%s \x1b[37m\n", identificador, mensaje);
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				pthread_mutex_lock(&mutexSolicitudes);
				colaSolicitudes[pos].sitio = 0;
				solicitudesEncoladas--;
				pthread_mutex_unlock(&mutexSolicitudes);
				pthread_exit(NULL);
			}
			sleep(4);
		} while(colaSolicitudes[pos].atendido == 0);
	}
	//Espera a que le hayan atendido y comprueba si ha sido atendido correctamente
	while (colaSolicitudes[pos].atendido == 1){
		sleep(3);
	}
	//Si ha sido atendido es decir 2 decide si va o no a una actividad
	if((colaSolicitudes[pos].atendido == 2) && (fin == 0)){
		aleatorio = aleatorios(1, 100);
		//Decide si va a la actividad o no
		if (aleatorio <= 50){
			//Intenta unirse a la actividad hasta que sea posible
			do{
				pthread_mutex_lock(&mutexCultural);
				//Si la lista cultural esta libre, se une a la actividad
				if((contadorCultural < 4) && (estadoCultural == 0)){
					//Si consigue entrar se pone la actividad a 1 para salir del bucle
					actividad = 1;
					//Se clona la solicitud en la cola cultura
					colaCultural[contadorCultural].ID = colaSolicitudes[pos].ID;
					colaCultural[contadorCultural].tipo = colaSolicitudes[pos].tipo;
					colaCultural[contadorCultural].sitio = 1;
					colaCultural[contadorCultural].posicion = contadorCultural;
					colaCultural[contadorCultural].atendido = colaSolicitudes[pos].atendido;
					contadorCultural++;
					//Se libera el espacio en la cola
					pthread_mutex_lock(&mutexSolicitudes);
					colaSolicitudes[pos].sitio = 0;
					solicitudesEncoladas--;
					pthread_mutex_unlock(&mutexSolicitudes);
					strcpy(mensaje, "esta lista para comenzar la actividad.");
					pthread_mutex_lock(&mutexLogs);
					writeLogMessage(identificador, mensaje);
					pthread_mutex_unlock(&mutexLogs);
					printf("%s \x1b[34m%s \x1b[37m\n",identificador, mensaje);
					//Si es el 4 hilo avisa al coordinador de que empiece la actividad
					if (contadorCultural == 4){
						pthread_cond_signal(&condCoordinador);
					}
					pthread_cond_wait(&condInicioActi, &mutexCultural);
					//Si se le ha despertado para acabar se va
					if((fin == 1) && (estadoCultural == 0)){
						pthread_mutex_unlock(&mutexCultural);
						pthread_exit(NULL);
					}
					if(contadorCultural == 4)
						sleep(3);
					contadorCultural--;
					colaCultural[contadorCultural].sitio = 0;
					//Si es el ultimo avisa al coordinador de que se termino la actividad
					strcpy(mensaje, "ha terminado la actividad.");
					pthread_mutex_lock(&mutexLogs);
					writeLogMessage(identificador, mensaje);
					pthread_mutex_unlock(&mutexLogs);
					printf("%s: %s\n",identificador, mensaje);
					if (contadorCultural == 0){
						pthread_cond_signal(&condCoordinador);
					}
					strcpy(mensaje, "abandona el programa.");
					pthread_mutex_lock(&mutexLogs);
					writeLogMessage(identificador, mensaje);
					pthread_mutex_unlock(&mutexLogs);
					printf("%s: \x1b[31m%s \x1b[37m\n",identificador, mensaje);
					pthread_mutex_unlock(&mutexCultural);
					pthread_exit(NULL);
				//Si la lista no esta libre duerme 3 segundos y lo vuelva a intentar
				}else{
					pthread_mutex_unlock(&mutexCultural);
					sleep(3);
					if(fin == 1){
						pthread_exit(NULL);
					}
				}	
			}while (actividad == 0);
		//Si es 1 el hilo no va a la actividad, libera su hueco en la cola y se va
		}else{ 
			strcpy(mensaje, "ha decidido no unirse a una actividad.");
			pthread_mutex_lock(&mutexLogs);
			writeLogMessage(identificador, mensaje);
			pthread_mutex_unlock(&mutexLogs);
			printf("%s: %s\n",identificador, mensaje);	
			//Se termima el hilo
			strcpy(mensaje, "abandona el programa.");
			pthread_mutex_lock(&mutexLogs);
			writeLogMessage(identificador, mensaje);
			pthread_mutex_unlock(&mutexLogs);
			printf("%s: \x1b[31m%s \x1b[37m\n",identificador, mensaje);
			pthread_mutex_lock(&mutexSolicitudes);
			colaSolicitudes[pos].sitio = 0;
			solicitudesEncoladas--;
			pthread_mutex_unlock(&mutexSolicitudes);
			pthread_exit(NULL);
		}
	}
	//Si no ha sido atendida correctamente abandona el programa siempre y cuando no se este terminando, que en ese caso sera expulsada.
	if(fin == 0){
		strcpy(mensaje, "abandona el programa.");
		pthread_mutex_lock(&mutexLogs);
		writeLogMessage(identificador, mensaje);
		pthread_mutex_unlock(&mutexLogs);
		printf("%s: \x1b[31m%s \x1b[37m\n",identificador, mensaje);
		pthread_mutex_lock(&mutexSolicitudes);
		colaSolicitudes[pos].sitio = 0;
		solicitudesEncoladas--;
		pthread_mutex_unlock(&mutexSolicitudes);
		pthread_exit(NULL);		
	}
}

/*
*Funcion que emula el comportamiento del coordinador, gestionando una actividad cultural.
*/
void *accionesCoordinadorSocial(void *arg){
	char identificador[30], mensaje[200];
	strcpy(identificador, "Coordinador");
	while (1){
		pthread_mutex_lock(&mutexCultural);
		//Espera a que se le avise que son 4
		pthread_cond_wait(&condCoordinador, &mutexCultural);
		//Si se esta terminando el programa se elimina
		if((fin == 1) && (contadorCultural == 0)){
			pthread_exit(NULL);
		}
		//Cerramos la cola cultural con la variable candado
		estadoCultural = 1;
		//sleep de 1 segundo para dar tiempo a que el que ha avisado a el coordinador pueda entrar en el wait
		sleep(1);
		//Escribimos en el log que comienza la actividad
		strcpy(mensaje, "comienza la actividad cultural.");
		pthread_mutex_lock(&mutexLogs);
		writeLogMessage(identificador, mensaje);
		pthread_mutex_unlock(&mutexLogs);
		printf("%s: \x1b[33m%s \x1b[37m\n",identificador, mensaje);
		//Avisa de que ya esta listo
		pthread_cond_broadcast(&condInicioActi); 
		//Espera a que termine la actividad
		pthread_cond_wait(&condCoordinador, &mutexCultural);
		//Una vez terminada se abre la cola de nuevo
		estadoCultural = 0;
		//Escribimos en el log que la actividad cultural ha terminado
		strcpy(mensaje, "la actividad cultural ha terminado.");
		pthread_mutex_lock(&mutexLogs);
		writeLogMessage(identificador, mensaje);
		pthread_mutex_unlock(&mutexLogs);
		printf("%s: \x1b[33m%s \x1b[37m\n",identificador, mensaje);
		pthread_mutex_unlock(&mutexCultural);
	}
}

/*
*Funcion para desplegar un menu y realizar asignaciones dinamicas.
*/
void manMenu(int sig){
int opcion, i, antiguo, totalAtendedores;
char identificador[30], mensaje[200];
pthread_t t1;
	printf("-------ASIGNACIONES DINAMICAS-------\n");
	printf("1) Reasignar solicitudes maximas\n");
	printf("2) Reasignar atendedores\n");
	scanf("%d",&opcion);
	if(opcion == 1){
		printf("Introduce el nuevo numero maximo de solicitudes:\n");
		scanf("%d",&opcion);
		if(opcion > maxSolicitudes){
			antiguo = maxSolicitudes;
			pthread_mutex_lock(&mutexSolicitudes);
			colaSolicitudes = realloc((struct Solicitudes *)colaSolicitudes,(opcion*sizeof(struct Solicitudes)));
			for(i = antiguo; i < opcion; i++){
				colaSolicitudes[i].ID = 0;
				colaSolicitudes[i].tipo = 0;
				colaSolicitudes[i].atendido = 0;
				colaSolicitudes[i].sitio = 0;
				colaSolicitudes[i].posicion = 0;
			}
			maxSolicitudes = opcion;
			printf("--CAMBIO REALIZADO CON EXITO (%d)--\n", maxSolicitudes);
			pthread_mutex_unlock(&mutexSolicitudes);
			strcpy(identificador, "Solicitudes");
			strcpy(mensaje, "El número de solicitudes ha sido actualizado.");
			pthread_mutex_lock(&mutexLogs);
			writeLogMessage(identificador, mensaje);
			pthread_mutex_unlock(&mutexLogs);
			
		}else{
			printf("--EL NUEVO NUMERO DE SOLICITUDES DEBE DE SER MAYOR QUE EL YA FIJADO (%d)--\n",colaSolicitudes[0].ID);
			/**
			strcpy(identificador, "Solicitudes");
			strcpy(mensaje, "El número de solicitudes debe ser mayor que el ya fijado.");
			pthread_mutex_lock(&mutexLogs);
			writeLogMessage(identificador, mensaje);
			pthread_mutex_unlock(&mutexLogs);
			*/
		}
	}else if(opcion == 2){
		printf("1) Modificar atendedores de INVITACION\n");
		printf("2) Modificar atendedores de QR\n");
		printf("3) Modificar atendedores PRO\n");
		scanf("%d",&opcion);
		if(opcion == 1){
			printf("Introduce el nuevo numero de atendedores de INVITACION\n");
			scanf("%d",&opcion);
			if(opcion > atendedores[0].ID){
				antiguo = atendedores[0].ID+atendedores[0].tipo+atendedores[0].solicitudesAtendidas;
				totalAtendedores = opcion+atendedores[0].tipo+atendedores[0].solicitudesAtendidas;
				atendedores[0].ID = opcion;
				atendedores = realloc((struct Atendedores *)atendedores, (totalAtendedores+1)*sizeof(struct Atendedores));
				for(i = antiguo+1; i<=totalAtendedores;i++){
					atendedores[i].tipo = 1;
					atendedores[i].ID = i;
					atendedores[i].solicitudesAtendidas = 0;
					pthread_create(&t1, NULL, accionesAtendedor, (void *)&atendedores[i]);
				}
				printf("--CAMBIO REALIZADO CON EXITO (%d)--\n",atendedores[0].ID);
				/**
				strcpy(identificador, "Atendedores");
				strcpy(mensaje, "El número de atendedores de invitacion ha sido actualizado.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				*/
				
			}else{
				printf("--EL NUEVO NUMERO DE ATENDEDORES DEBE DE SER MAYOR QUE EL YA FIJADO (%d)--\n",atendedores[0].ID);
				/**
				strcpy(identificador, "Atendedores");
				strcpy(mensaje, "El número de atendedores de invitacion debe ser mayor que el ya fijado.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				*/
			}	
		}else if(opcion == 2){
			printf("Introduce el nuevo numero de atendedores de QR:\n");
			scanf("%d",&opcion);
			if(opcion > atendedores[0].tipo){
				antiguo = atendedores[0].ID+atendedores[0].tipo+atendedores[0].solicitudesAtendidas;
				totalAtendedores = opcion+atendedores[0].ID+atendedores[0].solicitudesAtendidas;
				atendedores[0].tipo = opcion;
				atendedores = realloc((struct Atendedores *)atendedores, (totalAtendedores+1)*sizeof(struct Atendedores));
				for(i = antiguo+1; i<=totalAtendedores;i++){
					atendedores[i].tipo = 2;
					atendedores[i].ID = i;
					atendedores[i].solicitudesAtendidas = 0;
					pthread_create(&t1, NULL, accionesAtendedor, (void *)&atendedores[i]);
				}
				printf("--CAMBIO REALIZADO CON EXITO (%d)--\n",atendedores[0].tipo);
				/**
				strcpy(identificador, "Atendedores");
				strcpy(mensaje, "El número de atendedores de QR se ha actualizado.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				*/
			}else{
				printf("--EL NUEVO NUMERO DE SOLICITUDES DEBE DE SER MAYOR QUE EL YA FIJADO (%d)--\n",atendedores[0].tipo);
				/**
				strcpy(identificador, "Solciitudes");
				strcpy(mensaje, "El número de atendedores de QR debe ser mayor que el ya fijado.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				*/
			}
		}else if(opcion == 3){
			printf("Introduce el nuevo numero de atendedores PRO:\n");
			scanf("%d",&opcion);
			if(opcion > atendedores[0].solicitudesAtendidas){
				antiguo = atendedores[0].ID + atendedores[0].tipo + atendedores[0].solicitudesAtendidas;
				totalAtendedores = opcion + atendedores[0].ID + atendedores[0].tipo;
				atendedores[0].solicitudesAtendidas = opcion;
				atendedores = realloc((struct Atendedores *)atendedores, (totalAtendedores+1)*sizeof(struct Atendedores));
				for(i = antiguo+1; i<=totalAtendedores;i++){
					atendedores[i].tipo = 3;
					atendedores[i].ID = i;
					atendedores[i].solicitudesAtendidas = 0;
					pthread_create(&t1, NULL, accionesAtendedor, (void *)&atendedores[i]);
				}
				printf("--CAMBIO REALIZADO CON EXITO (%d)--\n",atendedores[0].solicitudesAtendidas);
				/**
				strcpy(identificador, "Atendedores");
				strcpy(mensaje, "El número de atendedores PRO se ha actualizado.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				*/
			}else{
				printf("--EL NUEVO NUMERO DE SOLICITUDES DEBE DE SER MAYOR QUE EL YA FIJADO (%d)--\n",atendedores[0].solicitudesAtendidas);
				/**
				strcpy(identificador, "Solicitudes");
				strcpy(mensaje, "El número de solicitudes debe ser mayor que el ya fijado.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				*/
				
			}
		}else{
			printf("--OPCION INCORRECTA--\n");	
		}
	}else{
		printf("--OPCION INCORRECTA--\n");
	}
}

/*
*Funcion manejadora que termina correctamente el programa cuando recibe SIGINT
*/
void manTerminacion(int sig){
	int i;
	struct sigaction ss = {0};
	char identificador[30], mensaje[200];
	strcpy(identificador, "Solicitud_");
	ss.sa_handler = manFin;
	fin = 1;
	//Codigo que ejecutare cuando el programa este terminandose
	if (sigaction(SIGUSR1, &ss, NULL) == -1){
		perror("Error en la llamada a sigaction");
		exit(-1);
	}
	if (sigaction(SIGUSR2, &ss, NULL) == -1){
		perror("Error en la llamada a sigaction");
		exit(-1);
	}
	if (sigaction(SIGINT, &ss, NULL) == -1){
		perror("Error en la llamada a sigaction");
		exit(-1);
	}
	if (sigaction(SIGPIPE, &ss, NULL) == -1){
		perror("Error en la llamada a sigaction");
		exit(-1);
	}
	//Acabamos todos los hilos en las solicitudes una vez que han sido atendidos correctamente
	while(solicitudesEncoladas != 0){
		for(i = 0; i < maxSolicitudes; i++){
			pthread_mutex_lock(&mutexSolicitudes);
			if(colaSolicitudes[i].sitio == 1){
				if(colaSolicitudes[i].atendido > 1){
					colaSolicitudes[i].sitio = 0;
					strcpy(identificador, "Solicitud_");
					creaIdentificador(identificador, colaSolicitudes[i].ID);
					strcpy(mensaje, "abandona el programa.");
					pthread_mutex_lock(&mutexLogs);
					writeLogMessage(identificador, mensaje);
					pthread_mutex_unlock(&mutexLogs);
					printf("%s: \x1b[31m%s \x1b[37m\n",identificador, mensaje);
					solicitudesEncoladas--;
				}
			}
			pthread_mutex_unlock(&mutexSolicitudes);		
		}
		sleep(1);
	}
	//Terminanos actividad correctamente
	if(estadoCultural == 0){
		while(contadorCultural != 0){
			for(i = 0; i < 4; i++){
				pthread_mutex_lock(&mutexCultural);
				if(colaCultural[i].sitio == 1){
					colaCultural[i].sitio = 0;
					strcpy(identificador, "Solicitud_");
					creaIdentificador(identificador, colaCultural[i].ID);
					strcpy(mensaje, "abandona el programa.");
					pthread_mutex_lock(&mutexLogs);
					writeLogMessage(identificador, mensaje);
					pthread_mutex_unlock(&mutexLogs);
					printf("%s: \x1b[31m%s \x1b[37m\n",identificador, mensaje);
					contadorCultural--;
				}
				pthread_mutex_unlock(&mutexCultural);
			}
			sleep(1);
		}
	}else{
		while(contadorCultural != 0){
			sleep(1);
		}	
	}
	//Se desbloque la condicion para poder eliminarla
	pthread_cond_signal(&condCoordinador);
	pthread_cond_destroy(&condCoordinador);
	pthread_cond_broadcast(&condInicioActi);
	pthread_cond_destroy(&condInicioActi);
	//Eliminamos todo lo necesario
	pthread_mutex_destroy(&mutexSolicitudes);
	pthread_mutex_destroy(&mutexCultural);
	pthread_mutex_destroy(&mutexLogs);
	free(colaSolicitudes);
	free(atendedores);
	//Salimos del programa con lo que terminaran todos sus hilos
	printf("---------------FIN DEL SERVICIO---------------\n");
	exit(0);
}

/*
*Funcion manejadora una vez iniciado al proceso de terminacion del programa para las señales SIGUSR1 y SIGUSR2
*/
void manFin(int sig){
	printf("APP: No se admiten mas solicitudes porque el programa está terminando.\n");
}

/*
*Funcion que busca una solicitud para atender dependiendo del tipo de atendendor que se la pasa como parametro.
*Primero crea dos arrays de solicitudes en los que se añadiran las solicitudes que no hayan sido atendidas aun dependiendo de su tipo
*Despues dependiendo de las prioridades de cada atendedor, dando prioridad a las de su tipo se buscara en los arrays correspondiendtes
*la solicitud que mas tiempo lleve esperando dependiendo de su ID y se devolvera su posicion en la cola al atendedor.
*
*Devuelve -1 si no se han encontrado solicitudes para atender
*/
int buscarSolicitud(int tipoAtendedor){
	int pos = 0, encontrado = 0, resultado = -1, *tipo1, *tipo2, i=0, contador1 = 0, contador2 = 0, aux, tamayo;
	tamayo = maxSolicitudes;
	tipo1 = malloc(tamayo*sizeof(int));
	tipo2 = malloc(tamayo*sizeof(int));
	for(i=0 ;i<tamayo ;i++){
		tipo1[i] = 0;
		tipo2[i] = 0;
	}
	for(i=0 ;i<tamayo;i++){
		if((colaSolicitudes[i].sitio == 1) && (colaSolicitudes[i].atendido == 0)){
			if(colaSolicitudes[i].tipo == 1){
				tipo1[contador1] = i;
				contador1++;
			}else{
				tipo2[contador2] = i;
				contador2++;
			}
		}
	}
	if((contador1 != 0) || (contador2 != 0)){
		if(tipoAtendedor == 1){
			if(contador1 != 0){
				resultado = buscaMasAntigua(tipo1, contador1);
			}else{ 
				resultado = buscaMasAntigua(tipo2, contador2);
			}
		}else if(tipoAtendedor == 2){
			if(contador2 != 0){
				resultado = buscaMasAntigua(tipo2, contador2);
			}else{
				resultado = buscaMasAntigua(tipo1, contador1);
			}
		}else{
			if(contador1 != 0){
				resultado = buscaMasAntigua(tipo1, contador1);
			}
			if(contador2 != 0){
				if(contador1 == 0){
					resultado = buscaMasAntigua(tipo2, contador2);
				}else{
					aux = buscaMasAntigua(tipo2, contador2);
					if(aux < resultado){
						resultado  = aux;
					}
				}
			}	
		}
	}
	free(tipo1);
	free(tipo2);	
	return resultado;
}

/*
*Funcion que dado un array con posiciones de solicitudes y un contador con el numero de solicitudes en el array, 
*elegira cual de ellas lleva mas tiempo esperando dependiendo del ID y devolvera la posicion en la que se encuentre.
*/
int buscaMasAntigua(int *solicitudes, int contador){
	int antiguo = colaSolicitudes[solicitudes[0]].ID, i, posicion = solicitudes[0];
	for(i = 0; i<contador;i++){
		if(colaSolicitudes[solicitudes[i]].ID < antiguo){
			antiguo = colaSolicitudes[solicitudes[i]].ID;
			posicion = solicitudes[i];
		}
	}
	return posicion;
}

/*
*Funcion que emula el comportamiento de un atendedor, que infinitamente comprueba si hay solicitudes para atender
*y las otorga un tipo de atencion especifico.
*/
void *accionesAtendedor(void *arg){
	struct Atendedores atendedor = *(struct Atendedores *)arg;
	int posicion = 0, tipoAtencion = 0, tiempoAtencion = 0;
	char identificador[30], mensaje[200], identificador2[30], mensaje2[200];
	strcpy(identificador, "Atendedor_");
	creaIdentificador(identificador, atendedor.ID);
	//Escribimos de que tipo de atendedor se trata
	if(atendedor.tipo == 1){
		strcpy(mensaje, "es un atendedor de invitaciones.");
		pthread_mutex_lock(&mutexLogs);
		writeLogMessage(identificador, mensaje);
		pthread_mutex_unlock(&mutexLogs);
		printf("%s: %s\n",identificador, mensaje);
	}else if(atendedor.tipo == 2){
		strcpy(mensaje, "es un atendedor de codigos QR.");
		pthread_mutex_lock(&mutexLogs);
		writeLogMessage(identificador, mensaje);
		pthread_mutex_unlock(&mutexLogs);
		printf("%s: %s\n",identificador, mensaje);
	}else{
		strcpy(mensaje, "es un atendedor PRO.");
		pthread_mutex_lock(&mutexLogs);
		writeLogMessage(identificador, mensaje);
		pthread_mutex_unlock(&mutexLogs);
		printf("%s: %s\n",identificador, mensaje);
	}
	//Comienzan a buscar solicitudes
	do{
		pthread_mutex_lock(&mutexSolicitudes);
		//busco una solicitud de su tipo para atender
		posicion = buscarSolicitud(atendedor.tipo);
		//Si ha encontrado alguna la atiendo
		if(posicion != -1){
			//Cambiamos el flag de atendido a 1
			colaSolicitudes[posicion].atendido = 1;
			//Escribimos en el log	
			strcpy(identificador2, "solicitud_");
			creaIdentificador(identificador2, colaSolicitudes[posicion].ID);
			sprintf(mensaje, "%s%s.", "esta atendiendo a la ",identificador2);
			pthread_mutex_lock(&mutexLogs);
			writeLogMessage(identificador, mensaje);
			pthread_mutex_unlock(&mutexLogs);
			printf("%s: %s\n",identificador, mensaje);
		}
		pthread_mutex_unlock(&mutexSolicitudes);
		if(posicion != -1){
			atendedor.solicitudesAtendidas = atendedor.solicitudesAtendidas +1;
			//Calculo el tipo de atencion y el tiempo de atencion
			tipoAtencion = aleatorios(1, 100);
			if (tipoAtencion <= 70){ //Atencion correcta
				tiempoAtencion = aleatorios(1, 4);
				//Escribimos en el log	
				sprintf(mensaje2, "%s%s.", "ha dado un tipo de atencion CORRECTA a la ",identificador2);
			}
			else if (tipoAtencion > 70 && tipoAtencion <= 90){ //Errores en datos personales
				tiempoAtencion = aleatorios(2, 6);
				sprintf(mensaje2, "%s%s.", "ha dado un tipo de atencion CON ERRORES EN LOS DATOS PERSONALES a la ",identificador2);
			}
			else{ //Antecedentes
				tiempoAtencion = aleatorios(6, 10);
				sprintf(mensaje2, "%s%s,%s.", "ha dado un tipo de atencion CON ANTECEDENTES a la ",identificador2," y será descartada");
			}
			//Dormimos el tiempo de atencion
			sleep(tiempoAtencion);
			//Escribimos en el log	
			sprintf(mensaje, "%s%s.", "ha terminado de atender a la ",identificador2);
			pthread_mutex_lock(&mutexLogs);
			writeLogMessage(identificador, mensaje);
			writeLogMessage(identificador, mensaje2);
			pthread_mutex_unlock(&mutexLogs);
			printf("%s: %s\n",identificador, mensaje);
			printf("%s: %s\n",identificador, mensaje2);
			//Cambiamos el flag de atendido
			pthread_mutex_lock(&mutexSolicitudes);
				//Si tenia antecedentes se cambia el flag a 4 y se apunta en el log
				if(tipoAtencion > 90){
					colaSolicitudes[posicion].atendido = 4;
				//Si no tenia antecedentes se cambia el flag a 2 y se apunta en el log
				}else{
					colaSolicitudes[posicion].atendido = 2;
				}
			pthread_mutex_unlock(&mutexSolicitudes);
			//Miramos si necesita tomar cafe
			if (atendedor.solicitudesAtendidas == 5){
				strcpy(mensaje, "se va a tomar cafe.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				printf("%s: %s\n",identificador, mensaje);
				sleep(10);
				atendedor.solicitudesAtendidas = 0;
				strcpy(mensaje, "ha vuelto de su descanso.");
				pthread_mutex_lock(&mutexLogs);
				writeLogMessage(identificador, mensaje);
				pthread_mutex_unlock(&mutexLogs);
				printf("%s: %s\n",identificador, mensaje);
			}
		}
		//Si no hay usuarios espero 1 segundo y vuelvo a buscar la primera solicitud
		sleep(1);
	}while(1);
}

void writeLogMessage(char *id, char *msg) {
 // Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	// Escribimos en el log
	logFile = fopen("registroTiempos.txt", "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
 }

void creaIdentificador(char *identificador, int numero){
	sprintf(identificador, "%s%d",identificador, numero);
}
