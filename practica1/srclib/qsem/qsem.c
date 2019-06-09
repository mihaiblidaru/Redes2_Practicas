/******************************************************************************
 * qLibc
 *
 * Copyright (c) 2010-2015 Seungyoung Kim.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/sem.h>
#include "qsem.h"

/**
 * Initialize semaphore
 *
 * @param keyfile   seed for generating unique IPC key
 * @param keyid     seed for generating unique IPC key
 * @param nsems     number of semaphores to initialize
 * @param recreate set to true to re-create semaphore if exists
 *
 * @return non-negative shared memory identifier if successful, otherwise returns -1
 *
 * @code
 *   int semid = qsem_init("/tmp/mydaemon.pid", 'q', 10, true);
 * @endcode
 */
int qsem_init(const char *keyfile, int keyid, int nsems, bool recreate) {
    key_t semkey;
    int semid;

    // generate unique key using ftok();
    if (keyfile != NULL) {
        semkey = ftok(keyfile, keyid);
        if (semkey == -1)
            return -1;
    } else {
        semkey = IPC_PRIVATE;
    }

    // create semaphores
    if ((semid = semget(semkey, nsems, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        if (recreate == false)
            return -1;

        // destroy & re-create
        if ((semid = qsem_getid(keyfile, keyid)) >= 0)
            qsem_free(semid);
        if ((semid = semget(semkey, nsems, IPC_CREAT | IPC_EXCL | 0666)) == -1)
            return -1;
    }

    // initializing
    int i;
    for (i = 0; i < nsems; i++) {
        struct sembuf sbuf;

        /* set sbuf */
        sbuf.sem_num = i;
        sbuf.sem_op = 1;
        sbuf.sem_flg = 0;

        /* initialize */
        if (semop(semid, &sbuf, 1) != 0) {
            qsem_free(semid);
            return -1;
        }
    }

    return semid;
}

/**
 * Get semaphore identifier by keyfile and keyid for the existing semaphore
 *
 * @param keyfile   seed for generating unique IPC key
 * @param keyid     seed for generating unique IPC key
 *
 * @return non-negative shared memory identifier if successful, otherwise returns -1
 */
int qsem_getid(const char *keyfile, int keyid) {
    int semid;

    /* generate unique key using ftok() */
    key_t semkey = ftok(keyfile, keyid);
    if (semkey == -1)
        return -1;

    /* get current semaphore id */
    if ((semid = semget(semkey, 0, 0)) == -1)
        return -1;

    return semid;
}

/**
 * Turn on the flag of semaphore then entering critical section
 *
 * @param semid     semaphore identifier
 * @param semno     semaphore number
 *
 * @return true if successful, otherwise returns false
 *
 * @note If the semaphore is already turned on, this will wait until released
 */
bool qsem_enter(int semid, int semno) {
    struct sembuf sbuf;

    /* set sbuf */
    sbuf.sem_num = semno;
    sbuf.sem_op = -1;
    sbuf.sem_flg = SEM_UNDO;

    /* lock */
    if (semop(semid, &sbuf, 1) != 0)
        return false;
    return true;
}


/**
 * Try to turn on the flag of semaphore. If it is already turned on, do not wait.
 *
 * @param semid     semaphore identifier
 * @param semno     semaphore number
 *
 * @return true if successful, otherwise(already turned on by other) returns false
 */
bool qsem_enter_nowait(int semid, int semno) {
    struct sembuf sbuf;

    /* set sbuf */
    sbuf.sem_num = semno;
    sbuf.sem_op = -1;
    sbuf.sem_flg = SEM_UNDO | IPC_NOWAIT;

    /* lock */
    if (semop(semid, &sbuf, 1) != 0)
        return false;
    return true;
}

/**
 * Force to turn on the flag of semaphore.
 *
 * @param semid     semaphore identifier
 * @param semno     semaphore number
 * @param maxwaitms maximum waiting micro-seconds to release
 * @param forceflag status will be stored, it can be NULL if you don't need this information
 *
 * @return true if successful, otherwise returns false
 *
 * @note
 * This will wait the semaphore to be released with in maxwaitms.
 * If it it released by locker normally with in maxwaitms, forceflag will be set to false.
 * But if maximum maxwaitms is exceed and the semaphore is released forcely, forceflag will
 * be set to true.
 */
bool qsem_enter_force(int semid, int semno, int maxwaitms, bool *forceflag) {
    int wait;
    for (wait = 0; wait < maxwaitms; wait += 10) {
        if (qsem_enter_nowait(semid, semno) == true) {
            if (forceflag != NULL)
                *forceflag = false;
            return true;
        }
        usleep(10 * 1000);  // sleep 10ms
    }

    while (true) {
        qsem_leave(semid, semno);
        if (qsem_enter_nowait(semid, semno) == true)
            break;
    }

    if (forceflag != NULL)
        *forceflag = true;
    return true;
}

/**
 * Turn off the flag of semaphore then leaving critical section
 *
 * @param semid     semaphore identifier
 * @param semno     semaphore number
 *
 * @return true if successful, otherwise returns false
 */
bool qsem_leave(int semid, int semno) {
    struct sembuf sbuf;

    /* set sbuf */
    sbuf.sem_num = semno;
    sbuf.sem_op = 1;
    sbuf.sem_flg = SEM_UNDO;

    /* unlock */
    if (semop(semid, &sbuf, 1) != 0)
        return false;
    return true;
}

/**
 * Get the status of semaphore
 *
 * @param semid     semaphore identifier
 * @param semno     semaphore number
 *
 * @return true for the flag on, false for the flag off
 */
int qsem_check(int semid, int semno) {
    return semctl(semid, semno, GETVAL, 0);
}

/**
 * Release semaphore to system
 *
 * @param semid     semaphore identifier
 *
 * @return true if successful, otherwise returns false
 */
bool qsem_free(int semid) {
    if (semid < 0)
        return false;
    if (semctl(semid, 0, IPC_RMID, 0) != 0)
        return false;
    return true;
}


/**
 * @brief Inicializa un semaforo a un valor dado.
 * 
 * @param semid id del array de semaforos
 * @param semno número del semaforo
 * @param semval valor deseado del semaforo
 * @return true si el cambio se ha realizado correctamente
 * @return false si el cambio no se ha realizado correctamente
 */
bool qsem_set(int semid, int semno, int semval) {
    int current_value = qsem_check(semid, semno);

    int diff = current_value - semval;
    struct sembuf sbuf;

    /* set sbuf */
    sbuf.sem_num = semno;
    sbuf.sem_op = diff;
    sbuf.sem_flg = SEM_UNDO;

    /* unlock */
    if (semop(semid, &sbuf, 1) != 0)
        return false;
    return true;

}
