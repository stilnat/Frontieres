**Frontières** est un échantillonneur granulaire.

*Ceci est une reprise non-officielle de la version 0.4 libre du logiciel [Borderlands](http://borderlands-granular.com/).  Voir [sa page sur linuxmao.org](http://linuxmao.org/borderlands). Vous trouverez des tas de notes concernant le logiciel originel en version 0.4 dans les fichiers [README.txt](README.txt) et [README-2.txt](README-2.txt).*


# Construction

[![Build Status](https://semaphoreci.com/api/v1/jpcima/frontieres/branches/master/badge.svg)](https://semaphoreci.com/jpcima/frontieres)

## Généralités

Le système de construction est cmake, et le cadriciel visuel est Qt5.


## Debian Stretch
 
Vous aurez besoin des paquets suivants : `cmake build-essential libasound2-dev libglu1-mesa-dev libjack-jackd2-dev` ou `libjack-dev libsndfile1-dev libsoxr-dev liblo-dev mesa-common-dev libpulse-dev` et `pkg-config`.

Ainsi que les paquets Qt5 suivants : `libqt5opengl5-dev, libqt5x11extras5-dev, qtbase5-dev, qttools5-dev,` et `qttools5-dev-tools`.

_(note : certains sont peut être non-nécessaires)_
