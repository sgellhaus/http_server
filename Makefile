http_server: main.c
	gcc -o http_server main.c
clean:
	rm -f http_server

test: http_server
	./http_server --dir test