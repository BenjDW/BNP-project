// Fonctionnement de inotify
https://manpages.ubuntu.com/manpages//focal/fr/man7/inotify.7.html

a chaque action (open,close,delete,create...) dans le systeme sa va appel en meme temps un event inotify

       L'API  inotify  fournit  un  mécanisme pour surveiller les événements au niveau des systèmes de fichiers.
       Inotify peut être utilisé  pour  surveiller  des  fichiers  individuels  ou  des  répertoires.  Quand  un
       répertoire  est  surveillé,  inotify  va  signaler des événements pour le répertoire lui-même et pour les
       fichiers de ce répertoire.

       Les appels système suivants sont utilisés avec cette interface de programmation :

       -  inotify_init(2) crée une instance inotify et renvoie un descripteur de fichier  se  référant  à  cette
          instance  inotify.  L'appel  système plus récent inotify_init1(2) est comme inotify_init(2), mais a un
          argument flags qui fournit un accès à des fonctionnalités supplémentaires.

       -  inotify_add_watch(2) manipule la « liste de surveillance » associée à  une  instance  inotify.  Chaque
          élément  (« watch »)  de  la  liste de surveillance indique le chemin d'un fichier ou d'un répertoire,
          avec un ensemble d'événements que le noyau doit surveiller pour le  fichier  indiqué  par  ce  chemin.
          inotify_add_watch(2)  crée  un  nouvel  élément de surveillance ou modifie un élément existant. Chaque
          élément a un unique « descripteur  de  surveillance »,  un  entier  renvoyé  par  inotify_add_watch(2)
          lorsque cet élément est créé.

       -  Quand  les événements ont lieu pour des fichiers et répertoires surveillés, ces événements sont rendus
          disponibles à l’application comme des données structurées qui peuvent être lues depuis le  descripteur
          de fichier inotify en utilisant read(2) (voir plus bas).

       -  inotify_rm_watch(2) retire un élément d'une liste de surveillance inotify.

       -  Quand tous les descripteurs de fichier se référant à une instance inotify ont été fermés (en utilisant
          close(2)), l'objet sous-jacent et ses ressources sont libérés pour être réutilisés par le noyau ; tous
          les éléments de surveillance associés sont automatiquement libérés.

          struct inotify_event {
               int      wd;       /* Descripteur de surveillance */
               uint32_t mask;     /* Masque d'événements */
               uint32_t cookie;   /* Cookie unique d'association des
                                     événements (pour rename(2)) */
               uint32_t len;      /* Taille du champ name */
               char     name[];   /* Nom optionnel terminé par un nul */
           };
           