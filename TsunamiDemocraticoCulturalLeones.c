#include <stdio.h>
#include <stdlib.h>
<<<<<<< HEAD
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

struct Solicitudes() {
	int ID;
	int atendido;
	int tipo;
};
Solicitudes colaSolicitudes[15];
Solicitudes colaCultural[4];
int contadorSolicitudes, contadorCultural;
struct Atendedores() {
	int ID;
	int tipo;
	int estado;
};
Atendedores atendedores[3];

void aleatorios(int min, int max);
void *accionesAtendedor(void *arg);
void *accionesCoordinadorSocial(void *arg);
void *nuevaSolicitud(void *arg);

int main(void) {
	pthread_t atendedor_1, atendedor_2, atendedor_3,coordinador;

	pthread_create (&atendedor_1, NULL, NULL);
	pthread_create (&atendedor_2, NULL, NULL);
	pthread_create (&atendedor_3, NULL, NULL);
	pthread_create (&coordinador, NULL, NULL);
}
void aleatorios(int main,int max) {
	return rand()%(max-min+1)+min;
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
=======


>>>>>>> 6ef0dfcffe0a8ecb20c610a05ce8b0f21cf667fd
