#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/*
 * Fonctions utilitaires internes (remplacement minimal de libft).
 *
 * Ces fonctions sont implémentées uniquement avec la bibliothèque standard C,
 * afin que le projet ne dépende plus de libft.
 */

/* Copie au plus dstsize-1 caractères de src dans dst, ajoute '\0'.
 * Retourne la longueur de src (même comportement que strlcpy BSD). */
size_t ft_strlcpy(char *dst, const char *src, size_t dstsize);

#endif /* UTIL_H */

