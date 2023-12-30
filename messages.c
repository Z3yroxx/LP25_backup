#include <messages.h>
#include <sys/msg.h>
#include <string.h>

// Functions in this file are required for inter processes communication

/*!
 * @brief send_file_entry sends a file entry, with a given command code
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @param cmd_code is the cmd code to process the entry.
 * @return the result of the msgsnd function
 * Used by the specialized functions send_analyze*
 */
int send_file_entry(int msg_queue, int recipient, files_list_entry_t *file_entry, int cmd_code) {
  
  //Vérification du paramètre file_entry
  if (file_entry == NULL) {
      printf("Erreur : file_entry est NULL\n");
      return -1;
  }

  //Mise à jour de la stucture avec les paramètres reçus par la fonction
  any_message_t msg;
  msg.list_entry.mtype = recipient;
  msg.list_entry.op_code = cmd_code;

  //Copie des données réceptionnées dans la structure
  memcpy(&msg.list_entry.payload, file_entry, sizeof(files_list_entry_t));

  //Envoi du message
  int snd = msgsnd(msg_queue, &msg, sizeof(msg) - sizeof(long), 0);

  //Vérification de la réussite ou non de l'envoi du message
  if (snd == -1) {
      printf("Erreur lors de l'envoi du message");
      return -1;
  }
  
  return snd;
}

/*!
 * @brief send_analyze_dir_command sends a command to analyze a directory
 * @param msg_queue is the id of the MQ used to send the command
 * @param recipient is the recipient of the message (mtype)
 * @param target_dir is a string containing the path to the directory to analyze
 * @return the result of msgsnd
 */
int send_analyze_dir_command(int msg_queue, int recipient, char *target_dir) {
  
  //Vérification du paramètre target_dir
  if (target_dir == NULL) {
      printf("Erreur : target_dir est NULL\n");
      return -1;
  }

  //Mise à jour de la stucture avec les paramètres reçus par la fonction
  analyze_dir_command_t cmd;
  cmd.mtype = recipient;
  cmd.op_code = 1;

  //Copie des données réceptionnées dans la structure
  strncpy(cmd.target, target_dir, sizeof(cmd.target) - 1);
  cmd.target[sizeof(cmd.target) - 1] = '\0'; // Assure la terminaison nulle

  //Envoi de la commande
  int snd = msgsnd(msg_queue, &cmd, sizeof(cmd) - sizeof(long), 0);

  //Vérification de la réussite ou non de l'envoi de la commande
  if (snd == -1) {
      printf("Erreur lors de l'envoi de la commande");
      return -1;
  }

  return snd;
}

// The 3 following functions are one-liners

/*!
 * @brief send_analyze_file_command sends a file entry to be analyzed
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_analyze_file_command(int msg_queue, int recipient, files_list_entry_t *file_entry) {
}

/*!
 * @brief send_analyze_file_response sends a file entry after analyze
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_analyze_file_response(int msg_queue, int recipient, files_list_entry_t *file_entry) {
}

/*!
 * @brief send_files_list_element sends a files list entry from a complete files list
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_files_list_element(int msg_queue, int recipient, files_list_entry_t *file_entry) {

    if (msg_queue == 0 || recipient == 0 || file_entry == NULL) {
        printf("Erreur : Paramètres non valides pour l'envoi de l'élément de liste de fichiers.\n");
        return -1;
    }

    files_list_entry_transmit_t entry_message;
    entry_message.mtype = recipient;
    entry_message.op_code = 'T'; // Op_code pour la transmission d'une entrée de liste
    entry_message.payload = *file_entry; // Copie de l'entrée de la liste de fichiers
    entry_message.reply_to = msg_queue; // MQ id de l'expéditeur pour la liste

    // Envoi du message
    int send_result = msgsnd(msg_queue, &entry_message, sizeof(files_list_entry_transmit_t) - sizeof(long), 0);

    if (send_result == -1) {
        perror("Erreur lors de l'envoi de l'élément de liste de fichiers");
        return -1; // En cas d'erreur
    }

    return send_result; // Renvoie le résultat de msgsnd
}

/*!
 * @brief send_list_end sends the end of list message to the main process
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */

int send_list_end(int msg_queue, int recipient) {

    simple_command_t message;
    message.mtype = recipient; // Modify the message type as needed
    message.message = 'L'; // 'L' for end of list (example)

    // Sending end of list message

    int msg = msgsnd(msg_queue, &message, sizeof(simple_command_t), 0);

    if (msg == -1) {
        perror("Error in sending end of list message");
        return errno;
    }

    return msg;

    return 0; // Success
}

/*!
 * @brief send_terminate_command sends a terminate command to a child process so it stops
 * @param msg_queue is the MQ id used to send the command
 * @param recipient is the target of the terminate command
 * @return the result of msgsnd
 */
int send_terminate_command(int msg_queue, int recipient) {

    if (msg_queue == 0 || recipient == 0) {
        printf("Erreur : File de messages ou destinataire non valide.\n");
        return -1;
    }

    files_list_end_t end_message;
    end_message.mtype = recipient; 
    end_message.op_code = 'T'; // Code de commande pour la fin de transmission de liste

    // Envoi du message
    int send_result = msgsnd(recipient, &end_message, sizeof(files_list_end_t) - sizeof(long), 0);
    
    if (send_result == -1) {
        perror("Erreur lors de l'envoi du message de fin de liste");
        return -1; // En cas d'erreur
    }

    return send_result; // Renvoie le résultat de msgsnd
}
/*!
 * @brief send_terminate_confirm sends a terminate confirmation from a child process to the requesting parent.
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */
int send_terminate_confirm(int msg_queue, int recipient) {

    if (msg_queue == 0 || recipient == 0) {
        printf("Erreur : file_entry = NULL\n");
        return -1; // Gestion du cas où les paramètres sont nuls ou invalides
    }

    // Création du message de confirmation de terminaison
    simple_command_t terminate_confirm_message;
    terminate_confirm_message.mtype = recipient;
    terminate_confirm_message.message = 'T'; // Valeur arbitraire pour indiquer la confirmation de terminaison

    // Envoi du message à la file de messages spécifiée
    int result = msgsnd(msg_queue, &terminate_confirm_message, sizeof(terminate_confirm_message.message), 0);


    if (result == -1) {
        perror("Erreur lors de l'envoi du message");
        return -1;
    }

    return result; // Retourne le résultat de msgsnd
}
