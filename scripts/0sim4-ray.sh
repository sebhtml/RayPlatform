source ../0sim4-parameters.sh
source ../0parameters.sh


mpirun -np 5 Ray -s $reads &> log
print-latex.sh $ref Ray-Contigs.fasta Ray
