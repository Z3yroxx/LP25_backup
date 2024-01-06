#include "processes.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <stdio.h>
#include <messages.h>
#include <file-properties.h>
#include <sync.h>
#include <string.h>
#include <errno.h>

// Ajout (Lorenzo) pour faire fonctionner make_process
#include <sys/types.h>
#include <sys/wait.h>

// Ajout (Lorenzo) pour faire fonctionner clean_processes
#include <signal.h>
#include <mqueue.h>

/*!
 * @brief prepare prepares (only when parallel is enabled) the processes used for the synchronization.
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the program processes context
 * @return 0 if all went good, -1 else
 */
int prepare(configuration_t *the_config, process_context_t *p_context) {
//fait par Talha
// Vérification des paramètres
    if (the_config == NULL || p_context == NULL) {
        fprintf(stderr, "Invalid arguments to prepare\n");
        return -1;
    }

    // Initialisation du contexte des processus
    p_context->num_processes = 0;
    p_context->pids = NULL;
    p_context->mq = -1;  // MQ non initialisée

    if (the_config->parallel_enabled) {
        // Initialisation des ressources nécessaires pour la communication entre les processus
        struct mq_attr mq_attributes;
        mq_attributes.mq_flags = 0;
        mq_attributes.mq_maxmsg = 10;  // Nombre maximum de messages dans la file
        mq_attributes.mq_msgsize = sizeof(any_message_t);
        mq_attributes.mq_curmsgs = 0;

        // Création de la file de messages
        p_context->mq = mq_open(the_config->mq_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &mq_attributes);
        if (p_context->mq == -1) {
            perror("Error creating message queue");
            return -1;
        }
    }

    return 0;  // Succès
}

/*!
 * @brief make_process creates a process and returns its PID to the parent
 * @param p_context is a pointer to the processes context
 * @param func is the function executed by the new process
 * @param parameters is a pointer to the parameters of func
 * @return the PID of the child process (it never returns in the child process)
 */
int make_process(process_context_t *p_context, process_loop_t func, void *parameters) {
    // fait par Lorenzo
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Erreur de création du processus");
        return -1;
    }

    if (pid == 0) {
        // Processus Fils
        func(parameters); // Execute la fonction dans le processus fils
        exit(EXIT_SUCCESS);
    } else {
        // Processus Parent
        p_context->pid = pid;
        waitpid(pid, NULL, 0); // Attends le processus fils

        return pid;
    }
}

/*!
 * @brief lister_process_loop is the lister process function (@see make_process)
 * @param parameters is a pointer to its parameters, to be cast to a lister_configuration_t
 */
void lister_process_loop(void *parameters) {
}

/*!
 * @brief analyzer_process_loop is the analyzer process function
 * @param parameters is a pointer to its parameters, to be cast to an analyzer_configuration_t
 */
void analyzer_process_loop(void *parameters) {
}

/*!
 * @brief clean_processes cleans the processes by sending them a terminate command and waiting to the confirmation
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the processes context
 */
void clean_processes(configuration_t *the_config, process_context_t *p_context) {
    // fait par Lorenzo
    if (the_config->parallel_enabled) {
        // Send terminate
        for (int i = 0; i < p_context->num_processes; ++i) {
            kill(p_context->pids[i], SIGTERM);
        }

        // Wait for responses
        for (int i = 0; i < p_context->num_processes; ++i) {
            int status;
            waitpid(p_context->pids[i], &status, 0);
            if (WIFEXITED(status)) {
                printf("Process %d exited with status %d\n", p_context->pids[i], WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Process %d terminated by signal %d\n", p_context->pids[i], WTERMSIG(status));
            }
            // Optionally, handle other termination conditions
        }

        // Free allocated memory
        free(p_context->pids);

        // Free the MQ
        if (p_context->mq != -1) {
            mq_close(p_context->mq);
            mq_unlink(the_config->mq_name);
        }

        // Réinitialisation du nombre de processus à 0
        p_context->num_processes = 0;
    }
}

