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
int contadorSolicitudes, contadorCultural, solicitudesEncoladas, fin, estadoCultural;

/*
*Structura que emula una solicitud a la aplicacion.
*ID: corresponde con el ID que tendra en la aplicacion.
*atendido: vale 0 si no esta atendido, 1 si si lo esta siendo o 2 si ya ha sido atendido CORRECTAMENTE, 3 si se encuentra en una actividad cultural o 4 si ha sido atendido erroneamente.
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

/*
*Structura que eumula un atendedor en la aplicacion.
*ID: corresponde con el ID que tendra en la apliacacion.
*tipo: corresponde con el tipo de de atendedor que es, 1 atendedor invitaciones, 2 atendedor QR, 3 atendedor PRO.
*solicitudesAtendidas: corresponde con el numero de solicitudes que han atendido para tomar cafe.
*/
struct Atendedores
{
	int ID;
	int tipo;
	int solicitudesAtendidas;
};

//Cola de solicitudes
struct Solicitudes colaSolicitudes[15];
//Cola de solicitudes en la actividad cultural
struct Solicitudes colaCultural[4];
//Lista con los atendedores
struct Atendedores atendedores[3];

//Declaracion de las funciones utilizadas.
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

//Funcion principal
int main(int argc, char argv[]){
	int i;
	struct sigaction ss = {0}, ss2 = {0};
	srand(time(NULL));
	//Inicializacion de mutex y variables condicion
	pthread_mutex_init(&mutexSolicitudes, NULL);
	pthread_mutex_init(&mutexCultural, NULL);
	pthread_cond_init(&condCoordinador, NULL);
	pthread_cond_init(&condInicioActi, NULL);
	//Creacion e inicializacion de los atendedores
	pthread_t atendedorIn, atendedorQR, atendedorPro, coordinador;
	for(i = 0; i < 3; i++){
		atendedores[i].tipo = i;
		atendedores[i].ID = i+1;
		atendedores[i].solicitudesAtendidas = 0;
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
	//creacion de los hilos atendedores y coordinador
	pthread_create(&atendedorIn, NULL, accionesAtendedor, (void *)&atendedores[0]);
	pthread_create(&atendedorQR, NULL, accionesAtendedor, (void *)&atendedores[1]);
	pthread_create(&atendedorPro, NULL, accionesAtendedor, (void *)&atendedores[2]);
	pthread_create(&coordinador, NULL, accionesCoordinadorSocial, NULL);
	//espera infinita a señales
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
	if (solicitudesEncoladas < 15){
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
		printf("Solicitud recibida %d, de tipo %d\n", colaSolicitudes[posicion].ID, colaSolicitudes[posicion].tipo);
	}else{
		printf("Señal ignorada\n");
	}
	pthread_mutex_unlock(&mutexSolicitudes);
}

/*
*Funcion que encuentra un sitio disponible en la cola de solicitudes, devuelve una posicion libre.
*/
int encuentraSitio(){
	int i = 0, encontrado = 0, posicion = -1;
	while ((i < 15) && (encontrado == 0)){
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
void *accionesSolicitud(void *arg)
{
	//Continua vale 0 si el hilo puede continuar o 1 si no puede.
	int pos = *(int *)arg, continua, aleatorio, actividad = 0, ID;
	ID = colaSolicitudes[pos].ID;
	sleep(4);
	//Si no ha sido atendido entra en el if
	if (colaSolicitudes[pos].atendido == 0){
		//Mientras no haya sido atendido realizara el bucle
		do{
			//Comprueba si ha debe descartarse
			if (colaSolicitudes[pos].tipo == 1){
				aleatorio = aleatorios(1, 100);
				if (aleatorio <= 10){
					continua = 1;
				}
			}else{
				aleatorio = aleatorios(1, 100);
				if (aleatorio <= 30){
					continua = 1;
				}
			}
			aleatorio = aleatorios(1, 100);
			if (aleatorio <= 15){
				continua = 1;
			}
			//Si debe descartarse abandona la cola, sino duerme 4 segundos y vuelva a comprobar si ha sido atendido
			if (continua == 1){
				printf("El hilo %d se va\n", colaSolicitudes[pos].ID);
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
					pthread_mutex_unlock(&mutexSolicitudes);
					//Si es el 4 hilo avisa al coordinador de que empiece la actividad
					if (contadorCultural == 4){
						pthread_cond_signal(&condCoordinador);
					}
					pthread_cond_wait(&condInicioActi, &mutexCultural);
					if(contadorCultural == 4)
						sleep(3);
					contadorCultural--;
					colaCultural[contadorCultural].sitio = 0;
					//Si es el ultimo avisa al coordinador de que se termino la actividad
					if (contadorCultural == 0){
						pthread_cond_signal(&condCoordinador);
					}
					printf("El hilo %d deja la actividad\n", ID);
					pthread_mutex_unlock(&mutexCultural);
				//Si la lista no esta libre duerme 3 segundos y lo vuelva a intentar
				}else{
					pthread_mutex_unlock(&mutexCultural);
					sleep(3);
				}
			}while (actividad == 0);
		//Si es 1 el hilo no va a la actividad, libera su hueco en la cola y se va
		}else{ 
			printf("El hilo %d no se ha unido a ninguna actividad y se va\n", ID);
		}
	}
	printf("->Fin hilo %d\n", ID);
	pthread_mutex_lock(&mutexSolicitudes);
	colaSolicitudes[pos].sitio = 0;
	solicitudesEncoladas--;
	pthread_mutex_unlock(&mutexSolicitudes);
	pthread_exit(NULL);
}

/*
*Funcion que emula el comportamiento del coordinador, gestionando una actividad cultural.
*/
void *accionesCoordinadorSocial(void *arg){
	while (1){
		pthread_mutex_lock(&mutexCultural);
		pthread_cond_wait(&condCoordinador, &mutexCultural); //Espera a que se le avise que son 4
		estadoCultural = 1;  
		printf("Comienza la actividad\n");
		sleep(1);
		pthread_cond_broadcast(&condInicioActi); //Avisa de que ya esta listo
		pthread_cond_wait(&condCoordinador, &mutexCultural); //Espera a que termine la actividad
		estadoCultural = 0;
		printf("Actividad terminada\n");
		pthread_mutex_unlock(&mutexCultural);
	}
}

/*
*Funcion manejadora que termina correctamente el programa cuando recibe SIGINT
*/
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

/*
*Funcion manejadora una vez iniciado al proceso de terminacion del programa para las señales SIGUSR1 y SIGUSR2
*/
void manFin(int sig){
	printf("señal no admitida\n");
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

/*
*Funcion que dado un array con posiciones de solicitudes y un contador con el numero de solicitudes en el array, 
*elegira cual de ellas lleva mas tiempo esperando dependiendo del ID y devolvera la posicion en la que se encuentre.
*/
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

/*
*Funcion que emula el comportamiento de un atendedor, que infinitamente comprueba si hay solicitudes para atender
*y las otorga un tipo de atencion especifico.
*/
void *accionesAtendedor(void *arg){
	struct Atendedores atendedor = *(struct Atendedores *)arg;
	int posicion = 0, tipoAtencion = 0, tiempoAtencion = 0;
	do{
		pthread_mutex_lock(&mutexSolicitudes);
		//busco una solicitud de su tipo para atender
		posicion = buscarSolicitud(atendedor.tipo);
		//Si ha encontrado alguna la atiendo
		if(posicion != -1){
			colaSolicitudes[posicion].atendido = 1;
			printf("Atendedor_%d esta atendiendo a la solicitud %d\n",atendedor.ID,colaSolicitudes[posicion].ID);
		}
		pthread_mutex_unlock(&mutexSolicitudes);
		if(posicion != -1){
			atendedor.solicitudesAtendidas = atendedor.solicitudesAtendidas +1;
			//Calculo el tipo de atencion y el tiempo de atencion
			tipoAtencion = aleatorios(1, 100);
			if (tipoAtencion <= 70){ //Atencion correcta
				tiempoAtencion = aleatorios(1, 4);
			}
			else if (tipoAtencion > 70 && tipoAtencion <= 90){ //Errores en datos personales
				tiempoAtencion = aleatorios(2, 6);
			}
			else{ //Antecedentes
				tiempoAtencion = aleatorios(6, 10);
			}
			//Dormimos el tiempo de atencion
			sleep(tiempoAtencion);
			//Cambiamos el flag de atendido
			pthread_mutex_lock(&mutexSolicitudes);
			printf("Atendedor_%d ha atendido a la solicitud %d\n",atendedor.ID,colaSolicitudes[posicion].ID);
				//Si tenia antecedentes se cambia el flag a 4 y se apunta en el log
				if(tipoAtencion > 90){
					printf("Atendedor_%d ha descartado por antecedentes a %d\n",atendedor.ID,colaSolicitudes[posicion].ID);
					colaSolicitudes[posicion].atendido = 4;
				//Si no tenia antecedentes se cambia el flag a 2 y se apunta en el log
				}else{
					printf("Atendedor_%d ha atendido a %d sin antecedentes\n",atendedor.ID,colaSolicitudes[posicion].ID);
					colaSolicitudes[posicion].atendido = 2;
				}
			pthread_mutex_unlock(&mutexSolicitudes);
			//Miramos si necesita tomar cafe
			if (atendedor.solicitudesAtendidas == 5){
				sleep(10);
				atendedor.solicitudesAtendidas = 0;
			}
		}
		//Si no hay usuarios espero 1 segundo y vuelvo a buscar la primera solicitud
		sleep(1);
	}while(1);
}
