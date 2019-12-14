#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int contadorSolicitudes, contadorCultural;
struct Solicitudes {
	int ID;
	int atendido;
	int tipo;
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
void *nuevaSolicitud(void *arg);

int main(void) {
int i;
struct sigaction ss = {0}, ss2 = {0};
pthread_t atendedorIn, atendedorQR, atendedorPro, coordinador;
srand(time(NULL));
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

void manSolicitud(int sig){

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
	printf("atendedor %d\n",*(int *) arg); 
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
void *nuevaSolicitud(void *arg) {
	/**
	Se comprueba si hay espacio
		if(espaciooK) -> se añade solicitud y contador+1
	 */
}

