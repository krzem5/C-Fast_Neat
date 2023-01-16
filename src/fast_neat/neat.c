#include <math.h>
#include <neat.h>
#include <stdio.h>
#include <stdlib.h>



#define BREED_MUTATION_CHANCE 0.5f
#define NODE_ADD_CHANCE 0.02f
#define WEIGHT_ADJUST_CHANCE 0.4f
#define WEIGHT_SET_CHANCE 0.1f
#define BIAS_ADJUST_CHANCE 0.3f
#define BIAS_SET_CHANCE 0.1f
#define MAX_STALE_FITNESS_DIFFERENCE 1e-6f



typedef struct _NEAT_MODEL_FILE_HEADER{
	unsigned int input_count;
	unsigned int output_count;
	unsigned int node_count;
	unsigned int edge_count;
} neat_model_file_header_t;



typedef struct _NEAT_MODEL_FILE_EDGE{
	unsigned int index;
	float weight;
} neat_model_file_edge_t;



static inline float _sigmoid(float x){
	return 1/(1+exp(-x));
}



static inline float _random_uniform(void){
	return ((float)rand())/RAND_MAX;
}



static inline unsigned int _random_int(unsigned int max){
	return rand()%max;
}



static void _adjust_genome_node_count(neat_genome_t* genome,unsigned int new_node_count){
	if (genome->node_count==new_node_count){
		return;
	}
	genome->node_count=new_node_count;
	genome->nodes=realloc(genome->nodes,new_node_count*sizeof(neat_genome_node_t));
	genome->edges=realloc(genome->edges,new_node_count*new_node_count*sizeof(neat_genome_edge_t));
}



void neat_init(unsigned int input_count,unsigned int output_count,unsigned int population,neat_t* out){
	out->input_count=input_count;
	out->output_count=output_count;
	out->population=population;
	out->_last_best_fitness=-1e8f;
	out->genomes=malloc(population*sizeof(neat_genome_t));
	unsigned int node_count=input_count+output_count;
	neat_genome_t* genome=out->genomes;
	for (unsigned int i=0;i<population;i++){
		genome->node_count=node_count;
		genome->nodes=malloc(node_count*sizeof(neat_genome_node_t));
		genome->edges=malloc(node_count*node_count*sizeof(neat_genome_edge_t));
		unsigned int l=0;
		for (unsigned int j=0;j<node_count;j++){
			(genome->nodes+j)->bias=0.0f;
			for (unsigned int k=0;k<node_count;k++){
				(genome->edges+l)->weight=0.0f;
				l++;
			}
		}
		for (unsigned int j=0;j<input_count;j++){
			l=j*node_count+input_count;
			for (unsigned int k=input_count;k<node_count;k++){
				(genome->edges+l)->weight=_random_uniform()*2-1;
				l++;
			}
		}
		genome++;
	}
}



void neat_deinit(const neat_t* neat){
	const neat_genome_t* genome=neat->genomes;
	for (unsigned int i=0;i<neat->population;i++){
		free(genome->nodes);
		free(genome->edges);
		genome++;
	}
	free(neat->genomes);
}



void neat_genome_evaluate(const neat_t* neat,const neat_genome_t* genome,const float* in,float* out){
	neat_genome_node_t* node=genome->nodes;
	for (unsigned int i=0;i<neat->input_count;i++){
		node->value=*in;
		node++;
		in++;
	}
	for (unsigned int i=neat->input_count;i<genome->node_count;i++){
		node->value=0.0f;
		node++;
	}
	node=genome->nodes+neat->input_count+neat->output_count;
	for (unsigned int i=neat->input_count+neat->output_count;i<genome->node_count;i++){
		float value=node->bias;
		unsigned int j=i;
		for (unsigned int k=0;k<genome->node_count;k++){
			value+=(genome->edges+j)->weight*(genome->nodes+k)->value;
			j+=genome->node_count;
		}
		node->value=_sigmoid(value);
		node++;
	}
	node=genome->nodes+neat->input_count;
	for (unsigned int i=neat->input_count;i<neat->input_count+neat->output_count;i++){
		float value=node->bias;
		unsigned int j=i;
		for (unsigned int k=0;k<genome->node_count;k++){
			value+=(genome->edges+j)->weight*(genome->nodes+k)->value;
			j+=genome->node_count;
		}
		node->value=_sigmoid(value);
		*out=node->value;
		node++;
		out++;
	}
}



const neat_genome_t* neat_update(neat_t* neat,float (*fitness_score_callback)(const neat_t*,const neat_genome_t*)){
	neat_genome_t* genome=neat->genomes;
	float average=0;
	const neat_genome_t* best_genome=NULL;
	for (unsigned int i=0;i<neat->population;i++){
		genome->fitness_score=fitness_score_callback(neat,genome);
		average+=genome->fitness_score;
		if (!best_genome||genome->fitness_score>best_genome->fitness_score){
			best_genome=genome;
		}
		genome++;
	}
	average/=neat->population;
	_Bool stale=fabs(neat->_last_best_fitness-average)<MAX_STALE_FITNESS_DIFFERENCE;
	neat->_last_best_fitness=average;
	neat_genome_t* start_genome=neat->genomes;
	neat_genome_t* end_genome=genome;
	genome=start_genome;
	for (unsigned int i=0;i<neat->population;i++){
		if (genome->fitness_score>=average){
			if (start_genome==genome){
				genome++;
			}
			else{
				neat_genome_t tmp=*genome;
				*genome=*start_genome;
				*start_genome=tmp;
			}
			start_genome++;
		}
		else{
			end_genome--;
			if (end_genome==genome){
				genome--;
			}
			else{
				neat_genome_t tmp=*genome;
				*genome=*end_genome;
				*end_genome=tmp;
			}
		}
	}
	if (stale||start_genome==neat->genomes){
		start_genome=neat->genomes+1;
	}
	neat_genome_t* child=start_genome;
	for (unsigned int idx=(start_genome-neat->genomes);idx<neat->population;idx++){
		const neat_genome_t* random_genome=neat->genomes+_random_int(idx);
		if (stale||_random_uniform()<=BREED_MUTATION_CHANCE){
			float value=_random_uniform()*(NODE_ADD_CHANCE+WEIGHT_ADJUST_CHANCE+WEIGHT_SET_CHANCE+BIAS_ADJUST_CHANCE+BIAS_SET_CHANCE);
			_Bool add_node=value<=NODE_ADD_CHANCE;
			_adjust_genome_node_count(child,random_genome->node_count+add_node);
			unsigned int k=0;
			unsigned int l=0;
			for (unsigned int i=0;i<random_genome->node_count;i++){
				for (unsigned int j=0;j<random_genome->node_count;j++){
					(child->edges+l)->weight=(random_genome->edges+k)->weight;
					k++;
					l++;
				}
				l+=add_node;
				(child->nodes+i)->bias=(random_genome->nodes+i)->bias;
			}
			if (add_node){
				unsigned int idx=_random_int(random_genome->node_count*random_genome->node_count);
				neat_genome_edge_t* edge=child->edges+idx;
				if (!edge->weight){
					edge->weight=_random_uniform()*2-1;
				}
				unsigned int i=idx/random_genome->node_count;
				unsigned int j=idx%random_genome->node_count;
				(child->edges+i*child->node_count+random_genome->node_count)->weight=1.0f;
				(child->edges+random_genome->node_count*child->node_count+j)->weight=edge->weight;
				(child->nodes+random_genome->node_count)->bias=0.0f;
				edge->weight=0.0f;
			}
			else if (value<=NODE_ADD_CHANCE+WEIGHT_ADJUST_CHANCE){
				(child->edges+_random_int(random_genome->node_count*random_genome->node_count))->weight+=_random_uniform()*2-1;
			}
			else if (value<=NODE_ADD_CHANCE+WEIGHT_ADJUST_CHANCE+WEIGHT_SET_CHANCE){
				(child->edges+_random_int(random_genome->node_count*random_genome->node_count))->weight=_random_uniform()*2-1;
			}
			else if (value<=NODE_ADD_CHANCE+WEIGHT_ADJUST_CHANCE+WEIGHT_SET_CHANCE+BIAS_ADJUST_CHANCE){
				(child->nodes+_random_int(random_genome->node_count))->bias+=_random_uniform()*2-1;
			}
			else{
				(child->nodes+_random_int(random_genome->node_count))->bias=_random_uniform()*2-1;
			}
		}
		else{
			const neat_genome_t* second_random_genome=neat->genomes+_random_int(idx);
			_adjust_genome_node_count(child,random_genome->node_count);
			unsigned int k=0;
			for (unsigned int i=0;i<random_genome->node_count;i++){
				for (unsigned int j=0;j<random_genome->node_count;j++){
					(child->edges+k)->weight=(i<second_random_genome->node_count&&j<second_random_genome->node_count&&_random_int(2)?random_genome->edges+k:second_random_genome->edges+i*second_random_genome->node_count+j)->weight;
					k++;
				}
				(child->nodes+i)->bias=((i<second_random_genome->node_count&&_random_int(2)?second_random_genome:random_genome)->nodes+i)->bias;
			}
		}
		child++;
	}
	return best_genome;
}



void neat_extract_model(const neat_t* neat,const neat_genome_t* genome,neat_model_t* out){
	out->input_count=neat->input_count;
	out->output_count=neat->output_count;
	out->node_count=genome->node_count;
	out->edge_count=0;
	out->nodes=malloc(out->node_count*sizeof(neat_model_node_t));
	out->edges=malloc(out->node_count*out->node_count*sizeof(neat_model_edge_t));
	const neat_genome_edge_t* genome_edge=genome->edges;
	neat_model_edge_t* edge=out->edges;
	for (unsigned int i=0;i<out->node_count;i++){
		(out->nodes+i)->bias=(genome->nodes+i)->bias;
		for (unsigned int j=0;j<out->node_count;j++){
			edge->weight=genome_edge->weight;
			if (edge->weight!=0.0f){
				out->edge_count++;
			}
			genome_edge++;
			edge++;
		}
	}
}



void neat_deinit_model(const neat_model_t* model){
	free(model->nodes);
	free(model->edges);
}



void neat_save_model(const neat_model_t* model,const char* file_path){
	FILE* file=fopen(file_path,"wb");
	neat_model_file_header_t header={
		model->input_count,
		model->output_count,
		model->node_count,
		model->edge_count
	};
	if (fwrite(&header,sizeof(header),1,file)!=1){
		goto _error;
	}
	const neat_model_node_t* node=model->nodes+model->input_count;
	for (unsigned int i=model->input_count;i<model->node_count;i++){
		if (fwrite(&(node->bias),sizeof(float),1,file)!=1){
			goto _error;
		}
		node++;
	}
	const neat_model_edge_t* edge=model->edges;
	for (unsigned int i=0;i<model->node_count*model->node_count;i++){
		if (edge->weight!=0.0f){
			neat_model_file_edge_t edge_data={
				i,
				edge->weight
			};
			if (fwrite(&edge_data,sizeof(neat_model_file_edge_t),1,file)!=1){
				goto _error;
			}
		}
		edge++;
	}
_error:
	fclose(file);
}
