/////////////////////////////////
//
//
//  Contains Miscellaneous APR numerics codes and output
//
//
//////////////////////////////////
#ifndef _misc_num_h
#define _misc_num_h

#include <algorithm>
#include <iostream>
#include <vector>
#include <fstream>

#include "../data_structures/Tree/PartCellStructure.hpp"
#include "../data_structures/Tree/ExtraPartCellData.hpp"
#include "filter_help/FilterLevel.hpp"

template<typename U,typename V>
void interp_img(Mesh_data<U>& img,PartCellData<uint64_t>& pc_data,ParticleDataNew<float, uint64_t>& part_new,ExtraPartCellData<V>& particles_int);

template<typename S>
void interp_depth_to_mesh(Mesh_data<uint8_t>& k_img,PartCellStructure<S,uint64_t>& pc_struct);

void create_y_data(ExtraPartCellData<uint16_t>& y_vec,ParticleDataNew<float, uint64_t>& part_new,PartCellData<uint64_t>& pc_data);

template<typename U,typename V>
void interp_slice(Mesh_data<U>& slice,std::vector<std::vector<std::vector<uint16_t>>>& y_vec,PartCellData<uint64_t>& pc_data,ParticleDataNew<float, uint64_t>& part_new,int dir,int num);

template<typename U,typename V>
void interp_slice(Mesh_data<U>& slice,PartCellData<uint64_t>& pc_data,ParticleDataNew<float, uint64_t>& part_new,ExtraPartCellData<V>& particles_int,int dir,int num);

void create_y_data(std::vector<std::vector<std::vector<uint16_t>>>& y_vec,PartCellStructure<float,uint64_t>& pc_struct);

template<typename V>
void filter_slice(std::vector<V>& filter,std::vector<V>& filter_d,ExtraPartCellData<V>& filter_output,Mesh_data<V>& slice,ExtraPartCellData<uint16_t>& y_vec,const int dir,const int num);

template<typename U>
std::vector<U> create_dog_filter(int size,float t,float K){
    //
    //  Bevan Cheeseman 2017
    //
    //  Difference of Gaussians Filter
    //
    //  https://nenadmarkus.com/posts/3D-DoG/
    //

    std::vector<U> filter;

    filter.resize(size*2 + 1,0);

    float del_x = 1;

    float pi = 3.14;

    float start_x = -size*del_x;

    float x = start_x;

    float factor1 = 1/pow(2*pi*pow(t,2),.5);
    float factor2 = 1/pow(2*pi*pow(K*t,2),.5);

    for (int i = 0; i < filter.size(); ++i) {
        filter[i] = factor1*exp(-pow(x,2)/(2*pow(t,2))) - factor2*exp(-pow(x,2)/(2*pow(K*t,2)));
        x += del_x;
    }

    float sum = 0;
    for (int i = 0; i < filter.size(); ++i) {
        sum += fabs(filter[i]);
    }

    for (int i = 0; i < filter.size(); ++i) {
        filter[i] = filter[i]/sum;
    }

    return filter;

}

template<typename S>
void interp_depth_to_mesh(Mesh_data<uint8_t>& k_img,PartCellStructure<S,uint64_t>& pc_struct){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes a pc_struct and interpolates the depth to the mesh
    //
    //
    
    //initialize dataset to interp to
    ExtraPartCellData<uint8_t> k_parts(pc_struct.part_data.particle_data);
    
    
    
    //initialize variables required
    uint64_t node_val_part; // node variable encoding part offset status information
    int x_; // iteration variables
    int z_; // iteration variables
    uint64_t j_; // index variable
    uint64_t curr_key = 0; // key used for accessing and particles and cells
    // Extra variables required
    //
    
    uint64_t status=0;
    uint64_t part_offset=0;
    uint64_t p;
    
    for(uint64_t i = pc_struct.pc_data.depth_min;i <= pc_struct.pc_data.depth_max;i++){
        //loop over the resolutions of the structure
        const unsigned int x_num_ = pc_struct.pc_data.x_num[i];
        const unsigned int z_num_ = pc_struct.pc_data.z_num[i];
        
        
#pragma omp parallel for default(shared) private(p,z_,x_,j_,node_val_part,curr_key,status,part_offset) if(z_num_*x_num_ > 100)
        for(z_ = 0;z_ < z_num_;z_++){
            //both z and x are explicitly accessed in the structure
            curr_key = 0;
            
            pc_struct.pc_data.pc_key_set_z(curr_key,z_);
            pc_struct.pc_data.pc_key_set_depth(curr_key,i);
            
            
            for(x_ = 0;x_ < x_num_;x_++){
                
                pc_struct.pc_data.pc_key_set_x(curr_key,x_);
                
                const size_t offset_pc_data = x_num_*z_ + x_;
                
                const size_t j_num = pc_struct.pc_data.data[i][offset_pc_data].size();
                
                //the y direction loop however is sparse, and must be accessed accordinagly
                for(j_ = 0;j_ < j_num;j_++){
                    
                    //particle cell node value, used here as it is requried for getting the particle neighbours
                    node_val_part = pc_struct.part_data.access_data.data[i][offset_pc_data][j_];
                    
                    if (!(node_val_part&1)){
                        //Indicates this is a particle cell node
                        
                        pc_struct.part_data.access_data.pc_key_set_j(curr_key,j_);
                        
                        status = pc_struct.part_data.access_node_get_status(node_val_part);
                        part_offset = pc_struct.part_data.access_node_get_part_offset(node_val_part);
                        
                        uint8_t depth = 2*i + (status == SEED);
                        
                        //loop over the particles
                        for(p = 0;p < pc_struct.part_data.get_num_parts(status);p++){
                            //first set the particle index value in the particle_data array (stores the intensities)
                            pc_struct.part_data.access_data.pc_key_set_index(curr_key,part_offset+p);
                            //get all the neighbour particles in (+y,-y,+x,-x,+z,-z) ordering
                            k_parts.get_part(curr_key) = depth;
                            
                        }
                        
                    } else {
                        // Inidicates this is not a particle cell node, and is a gap node
                    }
                    
                }
                
            }
            
        }
    }
    
    
    //now interpolate this to the mesh
    pc_struct.interp_parts_to_pc(k_img,k_parts);
    
    
}


template<typename S>
void interp_status_to_mesh(Mesh_data<uint8_t>& status_img,PartCellStructure<S,uint64_t>& pc_struct){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes a pc_struct and interpolates the depth to the mesh
    //
    //
    
    //initialize dataset to interp to
    ExtraPartCellData<uint8_t> status_parts(pc_struct.part_data.particle_data);
    
    
    
    //initialize variables required
    uint64_t node_val_part; // node variable encoding part offset status information
    int x_; // iteration variables
    int z_; // iteration variables
    uint64_t j_; // index variable
    uint64_t curr_key = 0; // key used for accessing and particles and cells
    // Extra variables required
    //
    
    uint64_t status=0;
    uint64_t part_offset=0;
    uint64_t p;
    
    for(uint64_t i = pc_struct.pc_data.depth_min;i <= pc_struct.pc_data.depth_max;i++){
        //loop over the resolutions of the structure
        const unsigned int x_num_ = pc_struct.pc_data.x_num[i];
        const unsigned int z_num_ = pc_struct.pc_data.z_num[i];
        
        
#pragma omp parallel for default(shared) private(p,z_,x_,j_,node_val_part,curr_key,status,part_offset) if(z_num_*x_num_ > 100)
        for(z_ = 0;z_ < z_num_;z_++){
            //both z and x are explicitly accessed in the structure
            curr_key = 0;
            
            pc_struct.pc_data.pc_key_set_z(curr_key,z_);
            pc_struct.pc_data.pc_key_set_depth(curr_key,i);
            
            
            for(x_ = 0;x_ < x_num_;x_++){
                
                pc_struct.pc_data.pc_key_set_x(curr_key,x_);
                
                const size_t offset_pc_data = x_num_*z_ + x_;
                
                const size_t j_num = pc_struct.pc_data.data[i][offset_pc_data].size();
                
                //the y direction loop however is sparse, and must be accessed accordinagly
                for(j_ = 0;j_ < j_num;j_++){
                    
                    //particle cell node value, used here as it is requried for getting the particle neighbours
                    node_val_part = pc_struct.part_data.access_data.data[i][offset_pc_data][j_];
                    
                    if (!(node_val_part&1)){
                        //Indicates this is a particle cell node
                        
                        
                        pc_struct.part_data.access_data.pc_key_set_j(curr_key,j_);
                        
                        status = pc_struct.part_data.access_node_get_status(node_val_part);
                        part_offset = pc_struct.part_data.access_node_get_part_offset(node_val_part);
                        
                        //loop over the particles
                        for(p = 0;p < pc_struct.part_data.get_num_parts(status);p++){
                            //first set the particle index value in the particle_data array (stores the intensities)
                            pc_struct.part_data.access_data.pc_key_set_index(curr_key,part_offset+p);
                            //get all the neighbour particles in (+y,-y,+x,-x,+z,-z) ordering
                            status_parts.get_part(curr_key) = status;
                            
                        }
                        
                    } else {
                        // Inidicates this is not a particle cell node, and is a gap node
                    }
                    
                }
                
            }
            
        }
    }
    
    
    //now interpolate this to the mesh
    pc_struct.interp_parts_to_pc(status_img,status_parts);
    
    
}

template<typename S,typename T>
void interp_extrapc_to_mesh(Mesh_data<T>& output_img,PartCellStructure<S,uint64_t>& pc_struct,ExtraPartCellData<T>& cell_data){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes a pc_struct and interpolates the depth to the mesh
    //
    //
    
    //initialize dataset to interp to
    ExtraPartCellData<T> extra_parts(pc_struct.part_data.particle_data);

    
    //initialize variables required
    uint64_t node_val_part; // node variable encoding part offset status information
    int x_; // iteration variables
    int z_; // iteration variables
    uint64_t j_; // index variable
    uint64_t curr_key = 0; // key used for accessing and particles and cells
    // Extra variables required
    //
    
    uint64_t status=0;
    uint64_t part_offset=0;
    uint64_t p;
    
    
    
    for(uint64_t i = pc_struct.pc_data.depth_min;i <= pc_struct.pc_data.depth_max;i++){
        //loop over the resolutions of the structure
        const unsigned int x_num_ = pc_struct.pc_data.x_num[i];
        const unsigned int z_num_ = pc_struct.pc_data.z_num[i];
        
        
#pragma omp parallel for default(shared) private(p,z_,x_,j_,node_val_part,curr_key,status,part_offset) if(z_num_*x_num_ > 100)
        for(z_ = 0;z_ < z_num_;z_++){
            //both z and x are explicitly accessed in the structure
            curr_key = 0;
            
            pc_struct.pc_data.pc_key_set_z(curr_key,z_);
            pc_struct.pc_data.pc_key_set_depth(curr_key,i);
            
            
            for(x_ = 0;x_ < x_num_;x_++){
                
                pc_struct.pc_data.pc_key_set_x(curr_key,x_);
                
                const size_t offset_pc_data = x_num_*z_ + x_;
                
                const size_t j_num = pc_struct.pc_data.data[i][offset_pc_data].size();
                
                //the y direction loop however is sparse, and must be accessed accordinagly
                for(j_ = 0;j_ < j_num;j_++){
                    
                    //particle cell node value, used here as it is requried for getting the particle neighbours
                    node_val_part = pc_struct.part_data.access_data.data[i][offset_pc_data][j_];
                    
                    if (!(node_val_part&1)){
                        //Indicates this is a particle cell node
                        
                        
                        pc_struct.part_data.access_data.pc_key_set_j(curr_key,j_);
                        
                        status = pc_struct.part_data.access_node_get_status(node_val_part);
                        part_offset = pc_struct.part_data.access_node_get_part_offset(node_val_part);
                        
                        float val = cell_data.get_val(curr_key);
                        
                        
                        //loop over the particles
                        for(p = 0;p < pc_struct.part_data.get_num_parts(status);p++){
                            //first set the particle index value in the particle_data array (stores the intensities)
                            pc_struct.part_data.access_data.pc_key_set_index(curr_key,part_offset+p);
                            //get all the neighbour particles in (+y,-y,+x,-x,+z,-z) ordering
                            extra_parts.get_part(curr_key) = val;
                            
                            
                        }
                    }
                    
                }
                
            }
            
        }
    }
    
    
    //now interpolate this to the mesh
    pc_struct.interp_parts_to_pc(output_img,extra_parts);
    
    
}
template<typename T>
void threshold_part(PartCellStructure<float,uint64_t>& pc_struct,ExtraPartCellData<T>& th_output,float threshold){
    //
    //
    //  Bevan Cheeseman 2016
    //
    //
    //
    
    uint64_t x_;
    uint64_t z_;
    uint64_t j_;
    
    th_output.initialize_structure_parts(pc_struct.part_data.particle_data);
    
    Part_timer timer;
    
    timer.verbose_flag = false;
    timer.start_timer("Threshold Loop");
    
    float num_repeats = 50;
    
    for(int r = 0;r < num_repeats;r++){
        
        for(uint64_t depth = (pc_struct.depth_max); depth >= (pc_struct.depth_min); depth--){
            
            const unsigned int x_num_ = pc_struct.pc_data.x_num[depth];
            const unsigned int z_num_ = pc_struct.pc_data.z_num[depth];
            
            //initialize layer iterators
            FilterLevel<uint64_t,float> curr_level;
            curr_level.set_new_depth(depth,pc_struct);
            
#pragma omp parallel for default(shared) private(z_,x_,j_) firstprivate(curr_level) if(z_num_*x_num_ > 100)
            for(z_ = 0;z_ < z_num_;z_++){
                //both z and x are explicitly accessed in the structure
                
                for(x_ = 0;x_ < x_num_;x_++){
                    curr_level.set_new_xz(x_,z_,pc_struct);
                    //the y direction loop however is sparse, and must be accessed accordinagly
                    for(j_ = 0;j_ < curr_level.j_num_();j_++){
                        
                        //particle cell node value, used here as it is requried for getting the particle neighbours
                        bool iscell = curr_level.new_j(j_,pc_struct);
                        
                        if (iscell){
                            curr_level.update_cell(pc_struct);
                            
                            for(int p = 0;p < pc_struct.part_data.get_num_parts(curr_level.status);p++){
                                th_output.data[depth][curr_level.pc_offset][curr_level.part_offset+p] = pc_struct.part_data.particle_data.data[depth][curr_level.pc_offset][curr_level.part_offset+p] > threshold ;
                            }
                            
                        } else {
                            curr_level.update_gap(pc_struct);
                        }
                    }
                    
                }
            }
            
            
        }
        
    }
    
    
    timer.stop_timer();
    float time = (timer.t2 - timer.t1)/num_repeats;
    
    std::cout << " Particle Threshold Size: " << pc_struct.get_number_parts() << " took: " << time << std::endl;
    
    
    
    
}

template<typename U>
void threshold_pixels(PartCellStructure<U,uint64_t>& pc_struct,uint64_t y_num,uint64_t x_num,uint64_t z_num){
    //
    //  Compute two, comparitive filters for speed. Original size img, and current particle size comparison
    //
    
    float threshold = 50;
    
    Mesh_data<U> input_data;
    Mesh_data<U> output_data;
    input_data.initialize((int)y_num,(int)x_num,(int)z_num,23);
    output_data.initialize((int)y_num,(int)x_num,(int)z_num,0);
    
    std::vector<float> filter;
    
    Part_timer timer;
    timer.verbose_flag = false;
    timer.start_timer("full previous filter");
    
    uint64_t filter_offset = 6;
    filter.resize(filter_offset*2 +1,1);
    
    uint64_t j = 0;
    uint64_t k = 0;
    uint64_t i = 0;
    
    float num_repeats = 50;
    
    for(int r = 0;r < num_repeats;r++){
        
#pragma omp parallel for default(shared) private(j,i,k)
        for(j = 0; j < z_num;j++){
            for(i = 0; i < x_num;i++){
                
                for(k = 0;k < y_num;k++){
                        
                    output_data.mesh[j*x_num*y_num + i*y_num + k] = input_data.mesh[j*x_num*y_num + i*y_num + k] < threshold;
                    
                }
            }
        }
        
    }
    
    
    timer.stop_timer();
    float time = (timer.t2 - timer.t1)/num_repeats;
    
    std::cout << " Pixel Threshold Size: " << (x_num*y_num*z_num) << " took: " << time << std::endl;
    
}
template<typename S>
void get_coord(const int dir,const CurrentLevel<S, uint64_t> &curr_level,const float step_size,int &dim1,int &dim2){
    //
    //  Bevan Cheeseman 2017
    //

    //calculate real coordinate




    if(dir != 1) {
        //yz case
            //y//z
        dim1 = curr_level.y * step_size;
        dim2 = curr_level.z * step_size;

    } else {
            //yx

        dim1 = curr_level.y * step_size;
        dim2 = curr_level.x * step_size;


    }

}

void get_coord(const int& dir,const int& y,const int& x,const int& z,const float& step_size,int &dim1,int &dim2){
    //
    //  Bevan Cheeseman 2017
    //

    //calculate real coordinate


    if(dir == 0){
        //yz
        dim1 = y * step_size;
        dim2 = z * step_size;
    } else if (dir == 1){
        //xy
        dim1 = x * step_size;
        dim2 = y * step_size;
    } else {
        //zy
        dim1 = z * step_size;
        dim2 = y * step_size;
    }

}
void get_coord_filter(const int& dir,const int& y,const int& x,const int& z,const float& step_size,int &dim1,int &dim2){
    //
    //  Bevan Cheeseman 2017
    //

    //calculate real coordinate


    if(dir != 1) {
        //yz case
        //y//z
        dim1 = (y + 0.5) * step_size;
        dim2 = (z + 0.5) * step_size;

    } else {
        //yx

        dim1 = (y + 0.5) * step_size;
        dim2 = (x + 0.5) * step_size;

    }

}




template<typename U,typename V>
void interp_slice(PartCellStructure<float,uint64_t>& pc_struct,ExtraPartCellData<V>& interp_data,int dir,int num){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes in a APR and creates piece-wise constant image
    //



    ParticleDataNew<float, uint64_t> part_new;
    //flattens format to particle = cell, this is in the classic access/part paradigm
    part_new.initialize_from_structure(pc_struct);

    //generates the nieghbour structure
    PartCellData<uint64_t> pc_data;
    part_new.create_pc_data_new(pc_data);

    //Genearate particle at cell locations, easier access
    ExtraPartCellData<float> particles_int;
    part_new.create_particles_at_cell_structure(particles_int);

    //iterator
    CurrentLevel<float,uint64_t> curr_level(pc_data);


    Mesh_data<U> slice;

    std::vector<unsigned int> x_num_min;
    std::vector<unsigned int> x_num;

    std::vector<unsigned int> z_num_min;
    std::vector<unsigned int> z_num;

    x_num.resize(pc_data.depth_max + 1);
    z_num.resize(pc_data.depth_max + 1);
    x_num_min.resize(pc_data.depth_max + 1);
    z_num_min.resize(pc_data.depth_max + 1);

    int x_dim = ceil(pc_struct.org_dims[0]/2.0)*2;
    int z_dim = ceil(pc_struct.org_dims[1]/2.0)*2;
    int y_dim = ceil(pc_struct.org_dims[2]/2.0)*2;


    if(dir != 1) {
        //yz case
        z_num = pc_data.z_num;

        for (int i = pc_data.depth_min; i <= pc_data.depth_max ; ++i) {
            x_num[i] = num/pow(2,pc_data.depth_max - i) + 1;
            z_num_min[i] = 0;
            x_num_min[i] = num/pow(2,pc_data.depth_max - i);
        }

        slice.initialize(pc_struct.org_dims[0],pc_struct.org_dims[2],1,0);


    } else {
        //yx case
        x_num = pc_data.x_num;

        for (int i = pc_data.depth_min; i <= pc_data.depth_max ; ++i) {
            z_num[i] = num/pow(2,pc_data.depth_max - i) + 1;
            x_num_min[i] = 0;
            z_num_min[i] = num/pow(2,pc_data.depth_max - i);
        }

        slice.initialize(pc_struct.org_dims[0],pc_struct.org_dims[1],1,0);

    }

    Part_timer timer;

    timer.verbose_flag = true;

    timer.start_timer("interp slice");


    int z_,x_,j_,y_;

    for(uint64_t depth = (part_new.access_data.depth_min);depth <= part_new.access_data.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_ = x_num[depth];
        const unsigned int z_num_ = z_num[depth];

        const unsigned int x_num_min_ = x_num_min[depth];
        const unsigned int z_num_min_ = z_num_min[depth];

        CurrentLevel<float, uint64_t> curr_level(pc_data);
        curr_level.set_new_depth(depth, part_new);

        const float step_size = pow(2,curr_level.depth_max - curr_level.depth);

#pragma omp parallel for default(shared) private(z_,x_,j_) firstprivate(curr_level) if(z_num_*x_num_ > 100)
        for (z_ = z_num_min_; z_ < z_num_; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = x_num_min_; x_ < x_num_; x_++) {

                curr_level.set_new_xz(x_, z_, part_new);

                for (j_ = 0; j_ < curr_level.j_num; j_++) {

                    bool iscell = curr_level.new_j(j_, part_new);

                    if (iscell) {
                        //Indicates this is a particle cell node
                        curr_level.update_cell(part_new);

                        float temp_int =  curr_level.get_val(particles_int);

                        int dim1 = 0;
                        int dim2 = 0;



                        get_coord(dir,curr_level,step_size,dim1,dim2);

                        //add to all the required rays

                        for (int k = 0; k < step_size; ++k) {
#pragma omp simd
                            for (int i = 0; i < step_size; ++i) {
                                //slice.mesh[dim1 + i + (dim2 + k)*slice.y_num] = temp_int;

                                slice(dim1 + i,(dim2 + k),1) = temp_int;
                            }
                        }


                    } else {

                        curr_level.update_gap();

                    }


                }
            }
        }
    }

    timer.stop_timer();

    debug_write(slice,"slice");


}
template<typename U>
void get_slices(PartCellStructure<float,uint64_t>& pc_struct){

    ParticleDataNew<float, uint64_t> part_new;
    //flattens format to particle = cell, this is in the classic access/part paradigm
    part_new.initialize_from_structure(pc_struct);

    //generates the nieghbour structure
    PartCellData<uint64_t> pc_data;
    part_new.create_pc_data_new(pc_data);

    //Genearate particle at cell locations, easier access
    ExtraPartCellData<float> particles_int;
    part_new.create_particles_at_cell_structure(particles_int);

    pc_data.org_dims = pc_struct.org_dims;
    part_new.access_data.org_dims = pc_struct.org_dims;
    part_new.particle_data.org_dims = pc_struct.org_dims;

    int dir = 0;
    int num = 800;

    int num_slices = 0;

    if(dir != 1){
        num_slices = pc_struct.org_dims[1];
    } else {
        num_slices = pc_struct.org_dims[2];
    }

    Mesh_data<U> slice;

    Part_timer timer;
    timer.verbose_flag = true;

    timer.start_timer("interp");

    for(int dir = 0; dir < 3;++dir) {

        if (dir != 1) {
            num_slices = pc_struct.org_dims[1];
        } else {
            num_slices = pc_struct.org_dims[2];
        }

        for (int i = 0; i < num_slices; ++i) {
            interp_slice(slice, pc_data, part_new, particles_int, dir, i);
        }
    }

    timer.stop_timer();

    interp_slice(slice,pc_data,part_new,particles_int,dir,150);

    debug_write(slice,"slice2");


    timer.start_timer("set up y");

    ExtraPartCellData<uint16_t> y_vec;

    create_y_data(y_vec,part_new,pc_data);


    timer.stop_timer();

    timer.start_timer("interp 2");

    for(int dir = 0; dir < 3;++dir) {

        if (dir != 1) {
            num_slices = pc_struct.org_dims[1];
        } else {
            num_slices = pc_struct.org_dims[2];
        }

        for (int i = 0; i < num_slices; ++i) {
            interp_slice_opt(slice, y_vec, part_new.particle_data, dir, i);
        }

    }

    timer.stop_timer();


    timer.start_timer("interp 2 old");

    for(int dir = 0; dir < 3;++dir) {

        if (dir != 1) {
            num_slices = pc_struct.org_dims[1];
        } else {
            num_slices = pc_struct.org_dims[2];
        }
        int i = 0;

#pragma omp parallel for default(shared) private(i) firstprivate(slice)
        for (i = 0; i < num_slices; ++i) {
            interp_slice(slice, y_vec, part_new.particle_data, dir, i);
        }

    }

    timer.stop_timer();

    interp_slice(slice, y_vec, part_new.particle_data, 0, 500);
    debug_write(slice,"slice3");

};


template<typename U,typename V>
void interp_img(Mesh_data<U>& img,PartCellData<uint64_t>& pc_data,ParticleDataNew<float, uint64_t>& part_new,ExtraPartCellData<V>& particles_int){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes in a APR and creates piece-wise constant image
    //

    img.initialize(pc_data.org_dims[0],pc_data.org_dims[1],pc_data.org_dims[2],0);

    int z_,x_,j_,y_;

    for(uint64_t depth = (part_new.access_data.depth_min);depth <= part_new.access_data.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_ = pc_data.x_num[depth];
        const unsigned int z_num_ = pc_data.z_num[depth];

        const unsigned int x_num_min_ = 0;
        const unsigned int z_num_min_ = 0;

        CurrentLevel<float, uint64_t> curr_level(pc_data);
        curr_level.set_new_depth(depth, part_new);

        const float step_size = pow(2,curr_level.depth_max - curr_level.depth);

#pragma omp parallel for default(shared) private(z_,x_,j_) firstprivate(curr_level) if(z_num_*x_num_ > 100)
        for (z_ = z_num_min_; z_ < z_num_; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = x_num_min_; x_ < x_num_; x_++) {

                curr_level.set_new_xz(x_, z_, part_new);

                for (j_ = 0; j_ < curr_level.j_num; j_++) {

                    bool iscell = curr_level.new_j(j_, part_new);

                    if (iscell) {
                        //Indicates this is a particle cell node
                        curr_level.update_cell(part_new);

                        int dim1 = curr_level.y * step_size;
                        int dim2 = curr_level.x * step_size;
                        int dim3 = curr_level.z * step_size;

                        //add to all the required rays

                        const float temp_int = curr_level.get_part(particles_int);

                        const int offset_max_dim1 = std::min((int) img.y_num, (int) (dim1 + step_size));
                        const int offset_max_dim2 = std::min((int) img.x_num, (int) (dim2 + step_size));
                        const int offset_max_dim3 = std::min((int) img.z_num, (int) (dim3 + step_size));

                        for (int q = dim3; q < offset_max_dim3; ++q) {

                            for (int k = dim2; k < offset_max_dim2; ++k) {
    #pragma omp simd
                                for (int i = dim1; i < offset_max_dim1; ++i) {
                                    img.mesh[i + (k) * img.y_num + q*img.y_num*img.x_num] = temp_int;
                                }
                            }
                        }


                    } else {

                        curr_level.update_gap();

                    }


                }
            }
        }
    }




}

template<typename V>
void set_zero_minus_1(ExtraPartCellData<V>& parts){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes in a APR and creates piece-wise constant image
    //



    int z_,x_,j_,y_;

    for(uint64_t depth = (parts.depth_min);depth < parts.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_ = parts.x_num[depth];
        const unsigned int z_num_ = parts.z_num[depth];

        const unsigned int x_num_min_ = 0;
        const unsigned int z_num_min_ = 0;


#pragma omp parallel for default(shared) private(z_,x_,j_) if(z_num_*x_num_ > 100)
        for (z_ = z_num_min_; z_ < z_num_; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = x_num_min_; x_ < x_num_; x_++) {

                const unsigned int pc_offset = x_num_*z_ + x_;

                std::fill(parts.data[depth][pc_offset].begin(),parts.data[depth][pc_offset].end(),0);

            }
        }
    }




}


template<typename U,typename V>
void interp_slice(Mesh_data<U>& slice,PartCellData<uint64_t>& pc_data,ParticleDataNew<float, uint64_t>& part_new,ExtraPartCellData<V>& particles_int,int dir,int num){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes in a APR and creates piece-wise constant image
    //


    std::vector<unsigned int> x_num_min;
    std::vector<unsigned int> x_num;

    std::vector<unsigned int> z_num_min;
    std::vector<unsigned int> z_num;

    x_num.resize(pc_data.depth_max + 1);
    z_num.resize(pc_data.depth_max + 1);
    x_num_min.resize(pc_data.depth_max + 1);
    z_num_min.resize(pc_data.depth_max + 1);


    if(dir != 1) {
        //yz case
        z_num = pc_data.z_num;

        for (int i = pc_data.depth_min; i <= pc_data.depth_max ; ++i) {
            x_num[i] = num/pow(2,pc_data.depth_max - i) + 1;
            z_num_min[i] = 0;
            x_num_min[i] = num/pow(2,pc_data.depth_max - i);
        }

        slice.initialize(pc_data.org_dims[0],pc_data.org_dims[2],1,0);


    } else {
        //yx case
        x_num = pc_data.x_num;

        for (int i = pc_data.depth_min; i <= pc_data.depth_max ; ++i) {
            z_num[i] = num/pow(2,pc_data.depth_max - i) + 1;
            x_num_min[i] = 0;
            z_num_min[i] = num/pow(2,pc_data.depth_max - i);
        }

        slice.initialize(pc_data.org_dims[0],pc_data.org_dims[1],1,0);

    }

    int z_,x_,j_,y_;

    for(uint64_t depth = (part_new.access_data.depth_min);depth <= part_new.access_data.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_ = x_num[depth];
        const unsigned int z_num_ = z_num[depth];

        const unsigned int x_num_min_ = x_num_min[depth];
        const unsigned int z_num_min_ = z_num_min[depth];

        CurrentLevel<float, uint64_t> curr_level(pc_data);
        curr_level.set_new_depth(depth, part_new);

        const float step_size = pow(2,curr_level.depth_max - curr_level.depth);

#pragma omp parallel for default(shared) private(z_,x_,j_) firstprivate(curr_level) if(z_num_*x_num_ > 100)
        for (z_ = z_num_min_; z_ < z_num_; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = x_num_min_; x_ < x_num_; x_++) {

                curr_level.set_new_xz(x_, z_, part_new);

                for (j_ = 0; j_ < curr_level.j_num; j_++) {

                    bool iscell = curr_level.new_j(j_, part_new);

                    if (iscell) {
                        //Indicates this is a particle cell node
                        curr_level.update_cell(part_new);



                        int dim1 = 0;
                        int dim2 = 0;

                        get_coord(dir,curr_level,step_size,dim1,dim2);

                        //add to all the required rays

//                        for (int k = 0; k < step_size; ++k) {
//pragma omp simd
//                            for (int i = 0; i < step_size; ++i) {
//                                slice.mesh[dim1 + i + (dim2 + k)*slice.y_num] = temp_int;
//
//                                //slice(dim1 + i,(dim2 + k),1) = temp_int;
//                            }
//                        }

                        //add to all the required rays

                        const float temp_int =  curr_level.get_val(particles_int);

                        const int offset_max_dim1 = std::min((int)slice.y_num,(int)(dim1 + step_size));
                        const int offset_max_dim2 = std::min((int)slice.x_num,(int)(dim2 + step_size));

                        if(depth == pc_data.depth_max){
                            int stop = 1;
                        }

                        for (int k = dim2; k < offset_max_dim2; ++k) {
#pragma omp simd
                            for (int i = dim1; i < offset_max_dim1; ++i) {
                                slice.mesh[ i + (k)*slice.y_num] = temp_int;

                            }
                        }


                    } else {

                        curr_level.update_gap();

                    }


                }
            }
        }
    }




}


void create_y_data(ExtraPartCellData<uint16_t>& y_vec,ParticleDataNew<float, uint64_t>& part_new,PartCellData<uint64_t>& pc_data){
    //
    //  Bevan Cheeseman 2017
    //
    //  Creates y index
    //


    y_vec.initialize_structure_parts(part_new.particle_data);

    int z_,x_,j_,y_;

    for(uint64_t depth = (part_new.access_data.depth_min);depth <= part_new.access_data.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_ = part_new.access_data.x_num[depth];
        const unsigned int z_num_ = part_new.access_data.z_num[depth];

        CurrentLevel<float, uint64_t> curr_level(pc_data);
        curr_level.set_new_depth(depth, part_new);

        const float step_size = pow(2,curr_level.depth_max - curr_level.depth);

#pragma omp parallel for default(shared) private(z_,x_,j_) firstprivate(curr_level) if(z_num_*x_num_ > 100)
        for (z_ = 0; z_ < z_num_; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = 0; x_ < x_num_; x_++) {

                curr_level.set_new_xz(x_, z_, part_new);

                int counter = 0;

                for (j_ = 0; j_ < curr_level.j_num; j_++) {

                    bool iscell = curr_level.new_j(j_, part_new);

                    if (iscell) {
                        //Indicates this is a particle cell node
                        curr_level.update_cell(part_new);

                        y_vec.data[depth][curr_level.pc_offset][counter] = curr_level.y;

                        counter++;
                    } else {

                        curr_level.update_gap();

                    }


                }
            }
        }
    }



}
template<typename U>
void interp_slice(Mesh_data<U>& slice,ExtraPartCellData<uint16_t>& y_vec,ExtraPartCellData<U>& particle_data,const int dir,const int num){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes in a APR and creates piece-wise constant image
    //
    //

    std::vector<unsigned int> x_num_min;
    std::vector<unsigned int> x_num;

    std::vector<unsigned int> z_num_min;
    std::vector<unsigned int> z_num;

    x_num.resize(y_vec.depth_max + 1,0);
    z_num.resize(y_vec.depth_max + 1,0);
    x_num_min.resize(y_vec.depth_max + 1,0);
    z_num_min.resize(y_vec.depth_max + 1,0);


    if(dir != 1) {
        //yz case
        z_num = y_vec.z_num;

        for (int i = y_vec.depth_min; i <= y_vec.depth_max ; ++i) {
            x_num[i] = num/pow(2,y_vec.depth_max - i) + 1;
            z_num_min[i] = 0;
            x_num_min[i] = num/pow(2,y_vec.depth_max - i);
        }

    } else {
        //dir = 1 case
        //yx case
        x_num = y_vec.x_num;

        for (int i = y_vec.depth_min; i <= y_vec.depth_max ; ++i) {
            z_num[i] = num/pow(2,y_vec.depth_max - i) + 1;
            x_num_min[i] = 0;
            z_num_min[i] = num/pow(2,y_vec.depth_max - i);
        }


    }

    if(dir == 0){
        //yz
        slice.initialize(y_vec.org_dims[0],y_vec.org_dims[2],1,0);
    } else if (dir == 1){
        //xy
        slice.initialize(y_vec.org_dims[1],y_vec.org_dims[0],1,0);

    } else if (dir == 2){
        //zy
        slice.initialize(y_vec.org_dims[2],y_vec.org_dims[0],1,0);

    }

    int z_,x_,j_,y_;

    for(uint64_t depth = (y_vec.depth_min);depth <= y_vec.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_max = x_num[depth];
        const unsigned int z_num_max = z_num[depth];

        const unsigned int x_num_min_ = x_num_min[depth];
        const unsigned int z_num_min_ = z_num_min[depth];

        const unsigned int x_num_ = y_vec.x_num[depth];

        const float step_size = pow(2,y_vec.depth_max - depth);

//#pragma omp parallel for default(shared) private(z_,x_,j_)
        for (z_ = z_num_min_; z_ < z_num_max; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = x_num_min_; x_ < x_num_max; x_++) {
                const unsigned int pc_offset = x_num_*z_ + x_;

                for (j_ = 0; j_ < y_vec.data[depth][pc_offset].size(); j_++) {

                    int dim1 = 0;
                    int dim2 = 0;

                    const int y = y_vec.data[depth][pc_offset][j_];

                    get_coord(dir,y,x_,z_,step_size,dim1,dim2);

                    const float temp_int = particle_data.data[depth][pc_offset][j_];

                    //add to all the required rays
                    const int offset_max_dim1 = std::min((int)slice.y_num,(int)(dim1 + step_size));
                    const int offset_max_dim2 = std::min((int)slice.x_num,(int)(dim2 + step_size));


                    for (int k = dim2; k < offset_max_dim2; ++k) {
#pragma omp simd
                        for (int i = dim1; i < offset_max_dim1; ++i) {
                            slice.mesh[ i + (k)*slice.y_num] = temp_int;

                        }
                    }


                }
            }
        }
    }



}
template<typename U>
void interp_slice_opt(Mesh_data<U>& slice,ExtraPartCellData<uint16_t>& y_vec,ExtraPartCellData<U>& particle_data,const int dir,const int num){
    //
    //  Bevan Cheeseman 2016
    //
    //  Takes in a APR and creates piece-wise constant image
    //
    //

    std::vector<unsigned int> x_num_min;
    std::vector<unsigned int> x_num;

    std::vector<unsigned int> z_num_min;
    std::vector<unsigned int> z_num;

    x_num.resize(y_vec.depth_max + 1,0);
    z_num.resize(y_vec.depth_max + 1,0);
    x_num_min.resize(y_vec.depth_max + 1,0);
    z_num_min.resize(y_vec.depth_max + 1,0);

    if(num == 0) {
        if (dir == 0) {
            //yz
            slice.initialize(y_vec.org_dims[0], y_vec.org_dims[2], 1, 0);
        } else if (dir == 1) {
            //xy
            slice.initialize(y_vec.org_dims[1], y_vec.org_dims[0], 1, 0);

        } else if (dir == 2) {
            //zy
            slice.initialize(y_vec.org_dims[2], y_vec.org_dims[0], 1, 0);

        }
    }


    if (dir != 1) {
        //yz case
        z_num = y_vec.z_num;

        for (int i = y_vec.depth_min; i < y_vec.depth_max; ++i) {

            int step = pow(2, y_vec.depth_max - i);
            int coord = num/step;

            int check1 = (coord*step);

            if(num == check1){
                x_num[i] = num/step + 1;
                x_num_min[i] = num/step;
            }
            z_num_min[i] = 0;
        }
        x_num[y_vec.depth_max] = num + 1;
        x_num_min[y_vec.depth_max] = num;

    } else {
        //yx case
        x_num = y_vec.x_num;

        for (int i = y_vec.depth_min; i < y_vec.depth_max; ++i) {

            int step = pow(2, y_vec.depth_max - i);
            int coord = num/step;

            int check1 = (coord*step);

            if(num == check1){
                z_num[i] = num/step + 1;
                z_num_min[i] = num/step;
            }
            x_num_min[i] = 0;
        }

        z_num[y_vec.depth_max] = num + 1;
        z_num_min[y_vec.depth_max] = num;

    }

    int z_=0;
    int x_=0;
    int j_ = 0;
    int y_ = 0;

    for(uint64_t depth = (y_vec.depth_min);depth <= y_vec.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_max = x_num[depth];
        const unsigned int z_num_max = z_num[depth];

        const unsigned int x_num_min_ = x_num_min[depth];
        const unsigned int z_num_min_ = z_num_min[depth];

        const unsigned int x_num_ = y_vec.x_num[depth];

        const float step_size = pow(2,y_vec.depth_max - depth);

#pragma omp parallel for default(shared) private(z_,x_,j_) if(dir == 1)
        for (z_ = z_num_min_; z_ < z_num_max; z_++) {
            //both z and x are explicitly accessed in the structure
#pragma omp parallel for default(shared) private(x_,j_) if(dir != 1)
            for (x_ = x_num_min_; x_ < x_num_max; x_++) {
                const unsigned int pc_offset = x_num_*z_ + x_;

                for (j_ = 0; j_ < y_vec.data[depth][pc_offset].size(); j_++) {

                    int dim1 = 0;
                    int dim2 = 0;

                    const int y = y_vec.data[depth][pc_offset][j_];

                    get_coord(dir,y,x_,z_,step_size,dim1,dim2);

                    const float temp_int = particle_data.data[depth][pc_offset][j_];

                    //add to all the required rays
                    const int offset_max_dim1 = std::min((int)slice.y_num,(int)(dim1 + step_size));
                    const int offset_max_dim2 = std::min((int)slice.x_num,(int)(dim2 + step_size));


                    for (int k = dim2; k < offset_max_dim2; ++k) {
                        for (int i = dim1; i < offset_max_dim1; ++i) {
                            slice.mesh[ i + (k)*slice.y_num] = temp_int;

                        }
                    }


                }
            }
        }
    }



}

template<typename U>
std::vector<U> shift_filter(std::vector<U>& filter){
    //
    //  Filter shift for non resolution part locations
    //
    //  Bevan Cheeseman 2017
    //

    std::vector<U> filter_d;

    filter_d.resize(filter.size() + 1,0);

    float factor = 0.5/4.0;

    filter_d[0] = filter[0]*factor;

    for (int i = 0; i < (filter.size() -1); ++i) {
        filter_d[i + 1] = filter[i]*factor + filter[i+1]*factor;
    }

    filter_d.back() = filter.back()*factor;

    return filter_d;

}


template<typename U>
ExtraPartCellData<U> filter_apr_by_slice(PartCellStructure<float,uint64_t>& pc_struct,std::vector<U>& filter,bool debug = false){

    ParticleDataNew<float, uint64_t> part_new;
    //flattens format to particle = cell, this is in the classic access/part paradigm
    part_new.initialize_from_structure(pc_struct);

    //generates the nieghbour structure
    PartCellData<uint64_t> pc_data;
    part_new.create_pc_data_new(pc_data);

    pc_data.org_dims = pc_struct.org_dims;
    part_new.access_data.org_dims = pc_struct.org_dims;

    part_new.particle_data.org_dims = pc_struct.org_dims;

    Mesh_data<U> slice;

    Part_timer timer;
    timer.verbose_flag = true;

    std::vector<U> filter_d = shift_filter(filter);


    ExtraPartCellData<uint16_t> y_vec;

    create_y_data(y_vec,part_new,pc_data);

    ExtraPartCellData<U> filter_output;
    filter_output.initialize_structure_parts(part_new.particle_data);

    ExtraPartCellData<U> filter_input;
    filter_input.initialize_structure_parts(part_new.particle_data);

    filter_input.data = part_new.particle_data.data;

    int num_slices = 0;


    timer.start_timer("filter all dir");

    for(int dir = 0; dir <1;++dir) {

        if (dir != 1) {
            num_slices = pc_struct.org_dims[1];
        } else {
            num_slices = pc_struct.org_dims[2];
        }

        if (dir == 0) {
            //yz
            slice.initialize(y_vec.org_dims[0], y_vec.org_dims[2], 1, 0);
        } else if (dir == 1) {
            //xy
            slice.initialize(y_vec.org_dims[1], y_vec.org_dims[0], 1, 0);

        } else if (dir == 2) {
            //zy
            slice.initialize(y_vec.org_dims[2], y_vec.org_dims[0], 1, 0);

        }

        //set to zero
        set_zero_minus_1(filter_output);

        int i = 0;
#pragma omp parallel for default(shared) private(i) firstprivate(slice) schedule(guided)
        for (i = 0; i < num_slices; ++i) {
            interp_slice(slice, y_vec, filter_input, dir, i);

            filter_slice(filter,filter_d,filter_output,slice,y_vec,dir,i);
        }

        //std::swap(filter_input,filter_output);
    }

    timer.stop_timer();

   // std::swap(filter_input,filter_output);

    if(debug == true) {

        Mesh_data<float> img;

        interp_img(img, pc_data, part_new, filter_output);

        for (int k = 0; k < img.mesh.size(); ++k) {
            img.mesh[k] = 10 * fabs(img.mesh[k]);
        }

        debug_write(img, "filter_img");
    }

    return filter_output;




};



template<typename V>
void filter_slice(std::vector<V>& filter,std::vector<V>& filter_d,ExtraPartCellData<V>& filter_output,Mesh_data<V>& slice,ExtraPartCellData<uint16_t>& y_vec,const int dir,const int num){

    int filter_offset = (filter.size()-1)/2;


    std::vector<unsigned int> x_num_min;
    std::vector<unsigned int> x_num;

    std::vector<unsigned int> z_num_min;
    std::vector<unsigned int> z_num;

    x_num.resize(y_vec.depth_max + 1,0);
    z_num.resize(y_vec.depth_max + 1,0);
    x_num_min.resize(y_vec.depth_max + 1,0);
    z_num_min.resize(y_vec.depth_max + 1,0);

    std::vector<bool> first_flag;
    first_flag.resize(y_vec.depth_max);

    if (dir != 1) {
        //yz case
        z_num = y_vec.z_num;

        for (int i = y_vec.depth_min; i < y_vec.depth_max; ++i) {

            int step = pow(2, y_vec.depth_max - i);
            int coord = num/step;

            int check1 = ((1.0*coord+.25)*step);
            int check2 = ((1.0*coord+.25)*step) + 1;

            if((num == check1) ){
                x_num[i] = num/step + 1;
                x_num_min[i] = num/step;
                first_flag[i] = false;
            } else if ((num == check2 )){
                x_num[i] = num/step + 1;
                x_num_min[i] = num/step;
                first_flag[i] = false;
            }
            z_num_min[i] = 0;
        }
        x_num[y_vec.depth_max] = num + 1;
        x_num_min[y_vec.depth_max] = num;

    } else {
        //yx case
        x_num = y_vec.x_num;

        for (int i = y_vec.depth_min; i < y_vec.depth_max; ++i) {

            int step = pow(2, y_vec.depth_max - i);
            int coord = num/step;

            int check1 = floor((1.0*coord+.25)*step);
            int check2 = floor((1.0*coord+.25)*step) + 1;

            if((num == check1) ){
                z_num[i] = num/step + 1;
                z_num_min[i] = num/step;
                first_flag[i] = false;
            } else if ((num == check2 )) {
                z_num[i] = num/step + 1;
                z_num_min[i] = num/step;
                first_flag[i] = false;
            }

            x_num_min[i] = 0;
        }

        z_num[y_vec.depth_max] = num + 1;
        z_num_min[y_vec.depth_max] = num;

    }

    int z_,x_,j_,y_;

    for(uint64_t depth = (y_vec.depth_min);depth <= y_vec.depth_max;depth++) {
        //loop over the resolutions of the structure
        const unsigned int x_num_max = x_num[depth];
        const unsigned int z_num_max = z_num[depth];

        const unsigned int x_num_min_ = x_num_min[depth];
        const unsigned int z_num_min_ = z_num_min[depth];

        const unsigned int x_num_ = y_vec.x_num[depth];

        const float step_size = pow(2,y_vec.depth_max - depth);

//#pragma omp parallel for default(shared) private(z_,x_,j_)
        for (z_ = z_num_min_; z_ < z_num_max; z_++) {
            //both z and x are explicitly accessed in the structure

            for (x_ = x_num_min_; x_ < x_num_max; x_++) {
                const unsigned int pc_offset = x_num_*z_ + x_;

                for (j_ = 0; j_ < y_vec.data[depth][pc_offset].size(); j_++) {

                    int dim1 = 0;
                    int dim2 = 0;

                    const int y = y_vec.data[depth][pc_offset][j_];

                    get_coord(dir,y,x_,z_,step_size,dim1,dim2);

                    if(depth == y_vec.depth_max) {

                        const int offset_max = std::min((int)(dim1 + filter_offset),(int)(slice.y_num-1));
                        const int offset_min = std::max((int)(dim1 - filter_offset),(int)0);

                        int f = 0;
                        V temp = 0;

                        for (int c = offset_min; c <= offset_max; c++) {

                            //need to change the below to the vector
                            temp += slice.mesh[c + (dim2) * slice.y_num] * filter[f];
                            f++;
                        }

                        filter_output.data[depth][pc_offset][j_] = temp;
                    } else {

                        const int offset_max = std::min((int)(dim1 + filter_offset + 1),(int)(slice.y_num-1));
                        const int offset_min = std::max((int)(dim1 - filter_offset),(int)0);

                        int f = 0;
                        V temp = 0;

                        const int dim2p = std::min(dim2 + 1,slice.x_num-1);
                        const int dim2m = dim2;

                        for (int c = offset_min; c <= offset_max; c++) {

                            //need to change the below to the vector
                            temp += (slice.mesh[c + (dim2m) * slice.y_num] + slice.mesh[c + (dim2p) * slice.y_num])  * filter_d[f];
                            f++;
                        }

                        filter_output.data[depth][pc_offset][j_] += temp;

                    }



                }
            }
        }
    }


}



#endif
    
    




