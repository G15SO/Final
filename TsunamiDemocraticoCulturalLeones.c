#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

pthread_mutex_t mutexSolicitudes, mutexCultural;
pthread_cond_t condCoordinador, condInicioActi;
int contadorSolicitudes, contadorCultural, solicitudesEncoladas, fin;

/*
*Structura que emula una solicitud a la aplicacion.
*ID: corresponde con el ID que tendra en la aplicacion.
*atendido: vale 0 si no esta atendido, 1 si si lo esta siendo o 2 si ya ha sido atendido.
*tipo: vale 1 si es de tipo invitacion o 2 si es de tipo qr.
*sitio: vale 0 si en esa posicion de la cola no hay alguien sentado o 1 si si lo hay.
*posicion: corresponde con la posicion que ocupa en la cola.
*/
struct Solicitudes
{
	int ID;
	int atendido;
	int tipo;
	int sitio;
	int posicion;
};

struct Atendedores
{
	int tipo;
	int solicitudesAtendidas;
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
void manFin(int sig);
int buscarSolicitud(int tipoAtendedor);
int buscaMasAntigua(int solicitudes[15], int contador);

int main(void)
{
	int i;
	struct sigaction ss = {0}, ss2 = {0};
	pthread_t atendedorIn, atendedorQR, atendedorPro, coordinador;
	srand(time(NULL));
	pthread_mutex_init(&mutexSolicitudes, NULL);
	pthread_mutex_init(&mutexCultural, NULL);
	pthread_cond_init(&condCoordinador, NULL);
	pthread_cond_init(&condInicioActi, NULL);
	contadorSolicitudes = 0;
	solicitudesEncoladas = 0;
	contadorCultural = 0;
	fin = 0;
	atendedores[0].tipo = 0;
	atendedores[1].tipo = 1;
	atendedores[2].tipo = 2;
	for (i = 0; i < 3; i++)
	{
		atendedores[i].solicitudesAtendidas = 0;
	}
	//tratamiento de SIGUSR1 para crear un nuevo hilo de tipo invitacion
	ss.sa_handler = manSolicitud;
	if (sigaction(SIGUSR1, &ss, NULL) == -1)
	{
		perror("Error\n");
		exit(-1);
	}
	//tratamiento de SIGUSR2 para crear un nuevo hilo de tipo qr
	if (sigaction(SIGUSR2, &ss, NULL) == -1)
	{
		perror("Error\n");
		exit(-1);
	}
	//tratamiento de SIGINT para finalizar el programa matando a todos los hilos
	ss2.sa_handler = manTerminacion;
	if (sigaction(SIGINT, &ss2, NULL) == -1)
	{
		perror("Error\n");
		exit(-1);
	}
	//creacion de los hilos atendedores y coordinador
	pthread_create(&atendedorIn, NULL, accionesAtendedor, (void *)&atendedores[0]);
	pthread_create(&atendedorQR, NULL, accionesAtendedor, (void *)&atendedores[1]);
	pthread_create(&atendedorPro, NULL, accionesAtendedor, (void *)&atendedores[2]);
	pthread_create(&coordinador, NULL, accionesCoordinadorSocial, NULL);
	//espera infinita a señales
	while (1)
	{
		pause();
	}
}

int aleatorios(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

/*Tengo que hacer un metodo nuevaSolicitud para gestionarlas o con la manejadora las va a ir gestionando solo.
Las gestiona FIFO o hay que cambiarlo.
Los hilos son distintos asi?
cuando hilos llegan a 15 id se resetea o sige 16,17...
*/
void manSolicitud(int sig)
{
	pthread_t t1;
	int posicion = 0, signal = -1;
	if (sig == SIGUSR1)
	{
		signal = 1;
	}
	if (sig == SIGUSR2)
	{
		signal = 2;
	}
	pthread_mutex_lock(&mutexSolicitudes);
	if (solicitudesEncoladas < 15)
	{
		contadorSolicitudes++;
		solicitudesEncoladas++;
		posicion = encuentraSitio();
		if (posicion == -1)
		{
			exit(-1);
		}
		colaSolicitudes[posicion].ID = contadorSolicitudes;
		colaSolicitudes[posicion].sitio = 1;
		colaSolicitudes[posicion].atendido = 0;
		colaSolicitudes[posicion].tipo = signal;
		colaSolicitudes[posicion].posicion = posicion;
		pthread_create(&t1, NULL, accionesSolicitud, (void *)&colaSolicitudes[posicion].posicion);
		printf("Solicitud recibida %d, de tipo %d\n", colaSolicitudes[posicion].ID, colaSolicitudes[posicion].tipo);
	}else{
		printf("Señal ignorada\n");
	}
	pthread_mutex_unlock(&mutexSolicitudes);
}

int encuentraSitio()
{
	int i = 0, encontrado = 0, posicion = -1;
	while ((i < 15) && (encontrado == 0))
	{
		if (colaSolicitudes[i].sitio == 0)
		{
			encontrado = 1;
			posicion = i;
		}
		i++;
	}
	return posicion;
}

void *accionesSolicitud(void *arg)
{
	//Continua vale 0 si el hilo puede continuar o 1 si no puede.
	int pos = *(int *)arg, continua, aleatorio, actividad = 0;
	sleep(4);
	if (colaSolicitudes[pos].atendido == 0)
	{
		do
		{
			if (colaSolicitudes[pos].tipo == 1)
			{
				aleatorio = aleatorios(1, 100);
				if (aleatorio <= 10)
				{
					continua = 1;
				}
			}
			else
			{
				aleatorio = aleatorios(1, 100);
				if (aleatorio <= 30)
				{
					continua = 1;
				}
			}
			aleatorio = aleatorios(1, 100);
			if (aleatorio <= 15)
			{
				continua = 1;
			}
			if (continua == 1)
			{
				printf("El hilo %d se va\n", colaSolicitudes[pos].ID);
				pthread_mutex_lock(&mutexSolicitudes);
				colaSolicitudes[pos].sitio = 0;
				solicitudesEncoladas--;
				pthread_mutex_unlock(&mutexSolicitudes);
				pthread_exit(NULL);
			}
			sleep(4);
		} while (colaSolicitudes[pos].atendido == 0);
	}
	printf("El hilo %d esta siendo atendido\n", colaSolicitudes[pos].ID);
	//Espera a que le hayan atendido
	while (colaSolicitudes[pos].atendido == 1)
	{
		sleep(2);
	}
	//Falta comprobar que si atenderle le han descartado<------------
	//Si ha sido atendido es decir 2 decide si va o no a una actividad seria asi??
	if (colaSolicitudes[pos].atendido == 2)
	{
		aleatorio = aleatorios(1, 100);
		//Si es 0 va a la actividad
		if (aleatorio <= 50)
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			do
			{
				pthread_mutex_lock(&mutexCultural);
				if (contadorCultural < 4)
				{
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
					pthread_mutex_unlock(&mutexSolicitudes);
					if (contadorCultural == 4)
					{
						pthread_cond_signal(&condCoordinador);
					}
					pthread_cond_wait(&condInicioActi, &mutexCultural);
					sleep(3);
					contadorCultural--;
					colaCultural[contadorCultural].sitio = 0;
					if (contadorCultural == 0)
					{
						pthread_cond_signal(&condCoordinador);
					}
					printf("El hilo %d deja la actividad\n", colaCultural[contadorCultural].ID);
					pthread_mutex_unlock(&mutexCultural);
				}
				else
				{
					pthread_mutex_unlock(&mutexCultural);
					sleep(3);
				}
			} while (actividad == 0);
		}
		else
		{ //Si es 1 el hilo no va a la actividad, libera su hueco en la cola y se va
			printf("El hilo %d se va\n", colaSolicitudes[pos].ID);
			pthread_mutex_lock(&mutexSolicitudes);
			colaSolicitudes[pos].sitio = 0;
			solicitudesEncoladas--;
			pthread_mutex_unlock(&mutexSolicitudes);
			pthread_exit(NULL);
		}
	}
	else
	{
		printf("El hilo %d fue descartado por...\n", 1);
	}
	printf("Fin hilo %d\n", colaSolicitudes[pos].ID);
	pthread_exit(NULL);
}

void *accionesCoordinadorSocial(void *arg)
{
	while (1)
	{
		pthread_mutex_lock(&mutexCultural);
		pthread_cond_wait(&condCoordinador, &mutexCultural); //Espera a que se le avise que son 4
		printf("Comienza la actividad\n");
		sleep(1);
		pthread_cond_signal(&condInicioActi);				 //Avisa de que ya esta listo
		pthread_cond_wait(&condCoordinador, &mutexCultural); //Espera a que termine la actividad
		printf("Actividad terminada\n");
		pthread_mutex_unlock(&mutexCultural);
	}
}

void manTerminacion(int sig)
{
	struct sigaction ss = {0};
	ss.sa_handler = manFin;
	if (sigaction(SIGUSR1, &ss, NULL) == -1)
	{
		perror("");
		exit(-1);
	}
}

void manFin(int sig)
{
	printf("señal no admitida\n");
}

int buscarSolicitud(int tipoAtendedor){
	int pos = 0, encontrado = 0, resultado = -1, tipo1[15], tipo2[15], i=0, contador1 = 0, contador2 = 0, aux;
	for(i;i<15;i++){
		tipo1[i] = 0;
		tipo2[i] = 0;
	}
	for(i=0;i<15;i++){
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
	return resultado;
}

int buscaMasAntigua(int solicitudes[15], int contador){
	int antiguo = colaSolicitudes[solicitudes[0]].ID, i, posicion = solicitudes[0];
	for(i = 0; i<contador;i++){
		if(colaSolicitudes[solicitudes[i]].ID < antiguo){
			antiguo = colaSolicitudes[solicitudes[i]].ID;
			posicion = solicitudes[i];
		}
	}
	return posicion;
}

void *accionesAtendedor(void *arg)
{
	struct Atendedores atendedor = *(struct Atendedores *)arg;
	int posicion = 0, tipoAtencion = 0, tiempoAtencion = 0;
	do
	{
		pthread_mutex_lock(&mutexSolicitudes);
		posicion = buscarSolicitud(atendedor.tipo);
		if(posicion != -1){
			colaSolicitudes[posicion].atendido = 1;
		}
		pthread_mutex_unlock(&mutexSolicitudes);
		if(posicion != -1){
			atendedor.solicitudesAtendidas = atendedor.solicitudesAtendidas +1;
			//Calculo el tipo de atencion y el tiempo de atencion
			tipoAtencion = aleatorios(1, 100);
			if (tipoAtencion <= 70)
			{ //Atencion correcta
				tiempoAtencion = aleatorios(1, 4);
			}
			else if (tipoAtencion > 70 && tipoAtencion <= 90)
			{ //Errores en datos personales
				tiempoAtencion = aleatorios(2, 6);
			}
			else
			{ //Antecedentes
				tiempoAtencion = aleatorios(6, 10);
			}
			//Dormimos el tiempo de atencion
			sleep(tiempoAtencion);
			//Cambiamos el flag de atendido
			pthread_mutex_lock(&mutexSolicitudes);
				if(tipoAtencion > 90){
					colaSolicitudes[posicion].atendido = 4;
				}else{
					colaSolicitudes[posicion].atendido = 2;
				}
			pthread_mutex_unlock(&mutexSolicitudes);
			//Miramos si necesita tomar cafe
			if (atendedor.solicitudesAtendidas == 5){
				sleep(10);
				atendedor.solicitudesAtendidas = 0;
			}
		}
		sleep(1);
	//Si no hay usuarios espero 1 segundo y vuelvo a buscar la primera solicitud
	}while(1);
}
