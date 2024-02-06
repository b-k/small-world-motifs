motifs.pdf: motifs.tex out/g1.png
	xelatex motifs
	bibtex motifs
	xelatex motifs
	xelatex motifs

onepass:
	docker run --rm -it --user=b --net=none -v "$(PWD)":/home/b/@ texbox xelatex "$$@"
# on the cmd line: docker run --rm -it --user=b --net=none -v "$PW)":/home/b/@ texbox xelatex "$@"

out/g1.png: g1.dot g2.dot g3.dot
	mkdir -p out
	neato -T png < g1.dot >  out/g1.png
	circo -T png < g2.dot >  out/g2.png
	neato -T png < g3.dot >  out/g3.png


motifs.tex: motifs.m4.org head
	-@printf "\n%%%%%%%%HEY, go edit the m4 document!!\n" > motifs.tex
	-m4 -P head motifs.m4.org | sed 's/λ/\\lambda{}/g; /^\*\*\*\*/d; s/^\*\*\*\(.*\)/\\paragraph{\1}/; s/^\*\*\(.*\)/\\subsection{\1}/; s/^\*\(.*\)/\\section{\1}/' >> motifs.tex
	-echo "\\\end{document}" >> motifs.tex

clean:
	rm -r motifs.log motifs.aux motifs.pdf motifs.tex motifs.blg motifs.bbl out
