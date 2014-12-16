
all: README.pdf

%.pdf: %.md
	pandoc $^ -t latex -o $@
