# hinfosvc
Jednoduchý minimalistický HTTP server poskytujúci rôzne informácie o systéme. 

Autor: Matej Matuška

## Preklad
Program je možné preložiť pomocou priloženého súboru Makefile píkazom `make`

## Použitie
Server je možné spustiť v príkazovom riadku príkazom:
`hinfosvc <číslo portu>`  

Server je možné ukončiť klávesovou skratkou Ctrl-C alebo klasickými UNIXovými príkazmi. Napríklad:  
`killall hinfosvc` alebo `kill <pid procesu hinfosvc>`

## Žiadosti
Server spracováva 3 tipy žiadostí:

- Meno hosťa `/hostname`, napríklad:  
  ```
$ curl <host:port>/hostname
subdomain.domain.com
```

- Meno procesora `/cpu-name`, napríklad:
  ```
$ curl <host:port>/cpu-name`   
Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz
 ```

- Záťaž procesora `/load`, napríklad:
  ```
$ curl <host:port>/load
15%
```

- Neplatná cesta, napríklad:
  ```
$ curl <host:port>/foo
404 Not Found
```

V príkladoch je použitý príkaz curl, ale na posielanie žiadostí na server je možné použiť aj iný nástroj ako napríklad `wget` alebo webový prehliadač