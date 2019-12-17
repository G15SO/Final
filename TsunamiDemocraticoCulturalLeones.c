#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

pthread_mutex_t mutexSolicitudes; 
int contadorSolicitudes, contadorCultural, solicitudesEncoladas;

/**
*Structura que emula una solicitud a la aplicacion.
*ID: corresponde con el ID que tendra en la aplicacion.
*atendido: vale 0 si no esta atendido, 1 si si lo esta siendo o 2 si ya ha sido atendido.
*tipo: vale 1 si es de tipo invitacion o 2 si es de tipo qr.
*sitio: vale 0 si en esa posicion de la cola no hay alguien sentado o 1 si si lo hay.
*posicion: corresponde con la posicion que ocupa en la cola.
*/
struct Solicitudes {
	int ID;
	int atendido;
	int tipo;
	int sitio;
	int posicion;
	//pthread_t thread;
};

struct Atendedores {
	int tipo;
	int estado;
};

struct Solicitudes colaSolicitudes[15];
struct Solicitudes colaCultural[4];
struct Atendedores atendedores[3];

int aleatorios(int min, int max);
void manSolicitud(int sig);
void manTerminacion(int sig);
void *accionesAtendedor(void *arg);
void *accionesCoordinadorSocial(void *arg);
void *accionesSolicitud(void *arg);
int encuentraSitio();



int main(void) {
int i;
struct sigaction ss = {0}, ss2 = {0};
pthread_t atendedorIn, atendedorQR, atendedorPro, coordinador;
srand(time(NULL));
	pthread_mutex_init(&mutexSolicitudes, NULL);
	contadorSolicitudes = 0;
	solicitudesEncoladas = 0;
	contadorCultural = 0;
	atendedores[0].tipo = 0;
	atendedores[1].tipo = 1;
	atendedores[2].tipo = 2;
	for(i = 0; i<3; i++){
		atendedores[i].estado = 0;
	}	
	//tratamiento de SIGUSR1 para crear un nuevo hilo de tipo invitacion
	ss.sa_handler = manSolicitud;
	if(sigaction(SIGUSR1,&ss, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	//tratamiento de SIGUSR2 para crear un nuevo hilo de tipo qr
	if(sigaction(SIGUSR2,&ss, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	//tratamiento de SIGINT para finalizar el programa matando a todos los hilos
	ss2.sa_handler = manTerminacion;
	if(sigaction(SIGINT,&ss2, NULL) == -1){
		perror("Error\n");
		exit(-1);
	}
	//creacion de los hilos atendedores y coordinador	
	pthread_create(&atendedorIn, NULL, accionesAtendedor, (void *) &atendedores[0].tipo);
	pthread_create(&atendedorQR, NULL, accionesAtendedor, (void *) &atendedores[1].tipo);
	pthread_create(&atendedorPro, NULL, accionesAtendedor, (void *) &atendedores[2].tipo);
	pthread_create(&coordinador, NULL, accionesCoordinadorSocial, NULL);
	//espera infinita a señales
	while(1){
		pause();
	}
}

int aleatorios(int min,int max) {
	return rand()%(max-min+1)+min;
}

/*Tengo que hacer un metodo nuevaSolicitud para gestionarlas o con la manejadora las va a ir gestionando solo.
Las gestiona FIFO o hay que cambiarlo.
Los hilos son distintos asi?
cuando hilos llegan a 15 id se resetea o sige 16,17...
*/
void manSolicitud(int sig){
pthread_t t1;
int posicion = 0, signal = -1;
	if(sig == SIGUSR1){
	 signal = 1;
	}
	if(sig == SIGUSR2){
	 signal = 2;
	}
	pthread_mutex_lock(&mutexSolicitudes);
	if(solicitudesEncoladas < 15){
		contadorSolicitudes++;
		solicitudesEncoladas++;
		posicion = encuentraSitio();
		if(posicion == -1){
			exit(-1);		
		}
		colaSolicitudes[posicion].ID = contadorSolicitudes;
		colaSolicitudes[posicion].sitio = 1;
		colaSolicitudes[posicion].atendido = 0;
		colaSolicitudes[posicion].tipo = signal;
		colaSolicitudes[posicion].posicion = posicion;
		//pthread_create(&colaSolicitudes[posicion].thread ,NULL ,accionesSolicitud, (void *)&posicion);
		pthread_create(&t1 ,NULL ,accionesSolicitud, (void *)&colaSolicitudes[posicion].posicion);
		printf("Solicitud recibida %d, de tipo %d\n",colaSolicitudes[posicion].ID, colaSolicitudes[posicion].tipo);
	}else{
		printf("Señal ignorada\n");
	}
	pthread_mutex_unlock(&mutexSolicitudes);

}

int encuentraSitio(){
int i = 0, encontrado = 0 , posicion = -1;
	while((i < 15) && (encontrado == 0)){
		if(colaSolicitudes[i].sitio == 0){
			encontrado = 1;
			posicion = i;
		}	
		i++;
	}
return posicion;
}

void *accionesSolicitud(void *arg){
//continua vale 0 si el hilo puede continuar o 1 si no puede.
int pos = *(int *)arg, continua, aleatorio;
sleep(4);
	if(colaSolicitudes[pos].atendido == 0){
		do{
			if(colaSolicitudes[pos].tipo == 1){
				aleatorio = aleatorios(1,100);
				if(aleatorio <= 10){
					continua = 1;
				}
			}else{
				aleatorio = aleatorios(1,100);
				if(aleatorio <= 30){
					continua = 1;
				}
			}
			aleatorio = aleatorios(1,100);
			if(aleatorio <= 15){
				continua = 1;
			}
			if(continua == 1){
				printf("El hilo %d se va\n",colaSolicitudes[pos].ID);
				pthread_mutex_lock(&mutexSolicitudes);
				colaSolicitudes[pos].sitio = 0;
				solicitudesEncoladas--;
				pthread_mutex_unlock(&mutexSolicitudes);
				pthread_exit(NULL);
			}	
			sleep(4);
		}while(colaSolicitudes[pos].atendido == 0);
	}
	printf("El hilo %d fue atendido\n",colaSolicitudes[pos].ID);
	//Esperar a que acaben de atenderle seria asi?
	while(colaSolicitudes[pos].atendido == 1){
		sleep(2);
	}

}

void manTerminacion(int sig){
exit(0);
}

void *accionesAtendedor(void *arg) {
	/**
	 * Buscar la primera solicitud que es la que lleva mas tiempo esperando
	 Miramos el tipo de señal y calculamos el tiempo de atencion
	 si SIGUSR1 ->Sistema
	  si SIGUSR2 -> QR
	 	Porcentaje ->random
		 0-6 -> Todo ok, sleep(1-4)
		 			Posible solicitud de actividad
					 if(solictud OK ) -> se libera hueco en cola de atencion y ocupa 1 hueco en cola cultural
					 if(!solicitud) -> Abandona el sistema, liberando un hueco en cola atencion


		 6-8 -> Error personales , sleep(2-6)
		 			Posible solicitud de actividad
						if(solictud OK ) -> se libera hueco en cola de atencion y ocupa 1 hueco en cola cultural
					 if(!solicitud) -> Abandona el sistema, liberando un hueco en cola atencion

		 restante -> Antecedentes, sleep(6-10), abandona cola atencion y sistema
		Cambio de flag
		sleep(T. atencion);
		
		cambio de flag
		toma de cafe
		Vuelta a comprobar la solicitud con prioridad.
	*/
	sleep(10);
	printf("atendedor %d\n",*(int *) arg); 
	sleep(6);
	colaSolicitudes[0].atendido = 1;
}
void *accionesCoordinadorSocial(void *arg) {
	/**
	Se espera por 4 listos(señales)
	contador +1;
	cuando haya cuatro -> Cierre de cola cultural
	Aviso de comienzo
	duracion de la actividad -> 3 segundos
	se informa de que han teminado y se resetea la cola
	*/
}

