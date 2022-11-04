#!/bin/bash
#SBATCH -A try22_Loreti2
#SBATCH --partition=skl_usr_prod
#SBATCH --job-name=SKL_batch_job
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=16
#SBATCH -o job.out
#SBATCH -e job.err
#SBATCH --gres=msrsafe
#SBATCH --exclusive

srun ./main