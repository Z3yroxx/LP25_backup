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
  return send_file_entry(msg_queue, recipient, file_entry, 1);

  //Attention : utilisation de la structure analyse_file_command_t
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
  return send_file_entry(msg_queue, recipient, file_entry, 1);
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
  return send_file_entry(msg_queue, recipient, file_entry, 1);
}

/*!
 * @brief send_list_end sends the end of list message to the main process
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */
int send_list_end(int msg_queue, int recipient) {

    //Vérification des paramètres
    if (msg_queue <= 0 || recipient <= 0) {
        printf("Error: Invalid message queue or recipient\n");
        return -1;
    }
    //Mise à jour de la stucture avec les paramètres reçus par la fonction
    simple_command_t end_message;
    end_message.mtype = recipient;
    end_message.message = END_OF_LIST;

    //Envoi du message
    int msg_sent = msgsnd(msg_queue, &end_message, sizeof(simple_command_t) - sizeof(long), 0);

    //Vérification de la réussite ou non de l'envoi de la commande
    if (msg_sent == -1) {
        perror("Error sending end of list message");
        return errno;
    }

    return msg_sent;
}


/*!
 * @brief send_terminate_command sends a terminate command to a child process so it stops
 * @param msg_queue is the MQ id used to send the command
 * @param recipient is the target of the terminate command
 * @return the result of msgsnd
 */
int send_terminate_command(int msg_queue, int recipient) {

    //Vérification des paramètres
    if (msg_queue <= 0 || recipient <= 0) {
        printf("Error: Message queue or recipient is invalid.\n");
        return -1;
    }
    //Mise à jour de la stucture avec les paramètres reçus par la fonction
    simple_command_t end_message;
    end_message.mtype = recipient; 
    end_message.op_code = 1; //

    //Envoi du message 
    int send_result = msgsnd(recipient, &end_message, sizeof(simple_command_t) - sizeof(long), 0);
    

    //Vérification de la réussite ou non de l'envoi de la commande
    if (send_result == -1) {
        perror("Error sending end of list message");
        return -1; // 
    }

    return send_result; 
}

/*!
 * @brief send_terminate_confirm sends a terminate confirmation from a child process to the requesting parent.
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */
int send_terminate_confirm(int msg_queue, int recipient) {

    //Vérification des paramètres
    if (msg_queue <= 0 || recipient <= 0) {
        printf("Error: msg_queue or recipient is NULL\n");
        return -1; 
    }

    //Mise à jour de la stucture avec les paramètres reçus par la fonction
    simple_command_t terminate_confirm_message;
    terminate_confirm_message.mtype = recipient;
    terminate_confirm_message.message = 'T'; // Valeur arbitraire pour indiquer la confirmation de la terminaison

    //Envoi du message
    int result = msgsnd(msg_queue, &terminate_confirm_message, sizeof(terminate_confirm_message.message), 0);

    //Vérification de la réussite ou non de l'envoi du message
    if (result == -1) {
        perror("Error sending message");
        return -1;
    }

    return result; 
}
