#include "ch.h"
#include "hal.h"
#include <chprintf.h>
#include <usbcfg.h>

#include <main.h>
#include <camera/po8030.h>

#include <process_image.h>

#define MARGIN 50


static float position_px = 0;


//semaphore
static BSEMAPHORE_DECL(image_ready_sem, TRUE);

static uint8_t min_val(uint8_t image[]){
	uint8_t small = 255;
	for (int i = MARGIN ; i < IMAGE_BUFFER_SIZE - MARGIN; i++){
		if (image[i] < small){
			small = image[i];
		}
	}
	return small;
}

static uint8_t max_val(uint8_t image[]){
	uint8_t big = 0;
	for (int i = MARGIN ; i < IMAGE_BUFFER_SIZE - MARGIN; i++){
		if (image[i] > big){
			big = image[i];
		}
	}
	return big;
}

static void image_info (uint8_t image[],uint16_t *width, uint16_t *position){
	uint8_t threshold = (max_val(image)+3*min_val(image))/4;
	uint16_t tempwidth = 0;
	uint16_t tempposition = 0;

	for (int i=MARGIN ; i < IMAGE_BUFFER_SIZE-MARGIN; i++){
			if (image[i]<threshold){
				(tempwidth)++;
				tempposition = i;//dernier pixel de la ligne
			}
			else if (tempwidth != 0){
				if (tempwidth < 5){//filtre passe haut
					tempwidth  = 0;
				}else{
					if(abs(tempposition-IMAGE_BUFFER_SIZE/2) < abs(*width-IMAGE_BUFFER_SIZE/2)){//prend la barre la plus grande
						*width = tempwidth;
						*position = tempposition;
					}
				}
			}
	}
}



static THD_WORKING_AREA(waCaptureImage, 256);
static THD_FUNCTION(CaptureImage, arg) {

    chRegSetThreadName(__FUNCTION__);
    (void)arg;

	//Takes pixels 0 to IMAGE_BUFFER_SIZE of the line 10 + 11 (minimum 2 lines because reasons)
	po8030_advanced_config(FORMAT_RGB565, 0, 10, IMAGE_BUFFER_SIZE, 2, SUBSAMPLING_X1, SUBSAMPLING_X1);
	dcmi_enable_double_buffering();
	dcmi_set_capture_mode(CAPTURE_ONE_SHOT);
	dcmi_prepare();


    while(1){
//    	systime_t t1 ;
//    	t1 = chVTGetSystemTime();
//		chprintf((BaseSequentialStream *) &SD3, "capture start\n ");

    	//starts a capture
		dcmi_capture_start();
//		chprintf((BaseSequentialStream *) &SD3, "wait image ready\n ");

		//waits for the capture to be done
		wait_image_ready();
		//signals an image has been captured
		chBSemSignal(&image_ready_sem);

//		chprintf((BaseSequentialStream *)&SDU1, "time = %d \n width = %d px\n position = %d px\n", chVTGetSystemTime()-t);

    }
}


static THD_WORKING_AREA(waProcessImage, 1024);
static THD_FUNCTION(ProcessImage, arg) {

    chRegSetThreadName(__FUNCTION__);
    (void)arg;

	uint8_t *img_buff_ptr;
	uint8_t image[IMAGE_BUFFER_SIZE] = {1};

	uint16_t width = 0;
	uint16_t position = 0;

    while(1){

    	width = 0;
    	 position = 0;

    	//waits until an image has been captured
        chBSemWait(&image_ready_sem);
//		chprintf((BaseSequentialStream *) &SD3, "sem received \n ");

		//gets the pointer to the array filled with the last image in RGB565
		img_buff_ptr = dcmi_get_last_image_ptr();

		//op�ration sur les bits :
		//l image est envoy�e en 2 x 8 bits, nouso n veut les 5 premier
		//donc on prend que les pairs et on shift de 3 pour les avoir

		for (int i=0 ; i < IMAGE_BUFFER_SIZE; i++){
			image[i] = *(img_buff_ptr+2*i) >> 3  ;
		}
		SendUint8ToComputer(image, IMAGE_BUFFER_SIZE);

		image_info(image,&width,&position);
		chprintf((BaseSequentialStream *) &SDU1, "position %d \n", position - IMAGE_BUFFER_SIZE/2);

//		chprintf((BaseSequentialStream *)&SDU1, "width = %d px position = %d px distance = %f cm\n",width,position-width/2,distance_cm);
    }
}



float get_position_px(void){
	return position_px;
}

void process_image_start(void){
	chThdCreateStatic(waProcessImage, sizeof(waProcessImage), NORMALPRIO, ProcessImage, NULL);
	chThdCreateStatic(waCaptureImage, sizeof(waCaptureImage), NORMALPRIO, CaptureImage, NULL);
}