source ../0sim4-parameters.sh
source ../0parameters.sh
velveth velvet $wordSize -long -fasta $reads &> log1
velvetg velvet -exp_cov auto -cov_cutoff auto &> log2
print-latex.sh $ref velvet/contigs.fa Velvet

