#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <errno.h>

#define MAX_RUNWAY 50  // Максимальное количество посадочных полос
#define MAX_AIRPLANES 500 // Максимальное количество самолетов

int localMaxRunways; // Введенное число
int localMaxAirplanes; // Введенное число
// \brief Массив полос
bool busy[MAX_RUNWAY];
// \brief Мьютекс массива полос
pthread_mutex_t busyMutex;
// \brief Мьютекс очереди самолетов
pthread_mutex_t airplanesQueueMutex;

// \brief Переменная для хранения освободившейся полосы
int justFreed = -1;
// \brief Условная переменная для передачи освободившейся полосы первому в очереди самолету
pthread_cond_t justFreedCond;
// \brief Мьютекс для условной переменной
pthread_mutex_t justFreedMutex; // Мьютексы для условной переменной

struct AirplaneInQueue {
    int currentAirplaneId;
    struct AirplaneInQueue *next;
};
// \brief Простая очередь самолетов
typedef struct AirplaneInQueue AirplaneInQueue;

// \brief Голова очереди самолетов
AirplaneInQueue *head;

// \brief Функция потока
void *airplane_thread(void *arg) {
    int airplane_id = *((int *) arg);
    while (1) {
        int selectedRunway = -1;

        printf("Самолет %d летит к аэропорту...\n", airplane_id);
        usleep(rand() % 1000000); // Имитация полета

        printf("Самолет %d проверяет очередь...\n", airplane_id);
        
        // Встаем в очередь даже если ее нету //
        pthread_mutex_lock(&airplanesQueueMutex);

        AirplaneInQueue* me = malloc(sizeof(AirplaneInQueue));
        me->currentAirplaneId = airplane_id;
        me->next = NULL;
        // очередь пустая
        if(head == NULL){
            head = me;
        } else {
            // Ищем конец очереди
            int count = 2;
            AirplaneInQueue *tail = head;
            while(NULL != tail->next){
                tail = tail->next;
                count++;
            }
            tail->next = me;
            printf("Самолет %d в очереди на позиции %d.\n", airplane_id, count);
        }

        pthread_mutex_unlock(&airplanesQueueMutex);

        // Перестраиваем очередь //
        pthread_mutex_lock(&airplanesQueueMutex);
        printf("Самолет %d первый на очереди, ищет свободную полосу...\n", airplane_id);
        if(NULL == head->next){ // Если это так, то значит мы единственные в очереди
            head = NULL;
        } else { // Иначе удаляем голову, устанавливаем голову за нами.
            AirplaneInQueue* toRemove = head;
            head = head->next;
            toRemove->next = NULL;
            free(toRemove);
        }
        pthread_mutex_unlock(&airplanesQueueMutex);

        // Сначала проходим по всем полосам, может быть есть свободная //
        pthread_mutex_lock(&justFreedMutex);
        pthread_mutex_lock(&busyMutex);
        for (int i = 0; i < localMaxRunways; i++) {                                                           
            if(busy[i] == false){
                // Здесь можно было бы пойти сразу в busy[i] = true, но тогда есть шанс гонки между
                // Этой частью кода и внутри обновления justFreed
                justFreed = i;
                break;
            }
        }
        // Разблокируем полосы, чтобы другой тред мог их освобождать
        pthread_mutex_unlock(&busyMutex);
        // Ждем justFreed.
        while(-1 == justFreed){
            pthread_cond_wait(&justFreedCond, &justFreedMutex);
        }
        // Забираем полосу //
        pthread_mutex_lock(&busyMutex);
        selectedRunway = justFreed;
        printf("Самолет %d входит на освободившуюся полосу №%d\n", airplane_id, justFreed);
        if(true == busy[selectedRunway]){
            perror("Полоса оказалась неразблокированной, самолеты столкнулись!");
            exit(EXIT_FAILURE);
        }
        busy[selectedRunway] = true;
        justFreed = -1;
        pthread_mutex_unlock(&busyMutex);
        pthread_mutex_unlock(&justFreedMutex);      
        
        // Забрали полосу, едем по ней //
        printf("Самолет %d зашел на полосу №%d...\n", airplane_id, selectedRunway);
        usleep(rand() % 1000000); // Имитация посадки
        
        // Освобождаем полосу //
        pthread_mutex_lock(&justFreedMutex);
        pthread_mutex_lock(&busyMutex);
        justFreed = selectedRunway;
        busy[selectedRunway] = false;
        pthread_mutex_unlock(&busyMutex);
        pthread_cond_signal(&justFreedCond);
        pthread_mutex_unlock(&justFreedMutex);
        
        printf("Самолет %d освободил полосу №%d.\n", airplane_id, selectedRunway);
    }
    return NULL;
}

int main() {
    // Запрос у пользователя количества посадочных полос
    printf("Введите число посадочных полос (Максимум %d): ", MAX_RUNWAY);
    scanf("%d", &localMaxRunways);

    if (localMaxRunways > MAX_RUNWAY) {
        printf("Ошибка: Число посадочных полос больше максимума.\n");
        return EXIT_FAILURE;
    }
    if (localMaxRunways < 1) {
        printf("Ошибка: Число посадочных полос меньше одного.\n");
        return EXIT_FAILURE;
    }

    // Запрос у пользователя количества самолетов
    printf("Введите число самолетов (Максимум %d): ", MAX_AIRPLANES);
    scanf("%d", &localMaxAirplanes);

    if (localMaxAirplanes > MAX_AIRPLANES) {
        printf("Ошибка: Число самолетов больше максимума.\n");
        return EXIT_FAILURE;
    }
    if (localMaxAirplanes < 1) {
        printf("Ошибка: Число самолетов меньше одного.\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&busyMutex, NULL);
    pthread_mutex_init(&airplanesQueueMutex, NULL);
    pthread_mutex_init(&justFreedMutex, NULL);
    pthread_cond_init(&justFreedCond, NULL);
    // Инициализация посадочных полос
    for (int i = 0; i < localMaxAirplanes; i++) {
        busy[i] = false;
    }

    // Создание потоков для самолетов
    pthread_t airplanes[localMaxAirplanes];
    for (int i = 0; i < localMaxAirplanes; i++) {
        int *airplane_id = (int *) malloc(sizeof(int));
        *airplane_id = i + 1;
        pthread_create(&airplanes[i], NULL, airplane_thread, (void *) airplane_id);
    }
    pthread_join(airplanes[0], NULL); // Никогда не выйдет
    return 0;
}
