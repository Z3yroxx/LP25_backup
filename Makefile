# Compiler
CC = gcc
# Options de compilation
CFLAGS = -Wall -Wextra -g
# Fichiers source
SRCS = configuration.c file-properties.c files-list.c main.c sync.c
# Fichiers objets
OBJS = $(SRCS:.c=.o)
# Bibliothèques supplémentaires, si nécessaire
LIBS =
# Exécutable final
EXEC = my_program

# Règle pour construire l'exécutable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC) $(LIBS)

# Règle pour chaque fichier objet
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Règle de nettoyage
clean:
	rm -f $(OBJS) $(EXEC)
