#include <camera.h>
#include <dvp.h>
#include <fpioa.h>
#include <gpio.h>
#include <gpio_common.h>
#include <image_process.h>
#include <kpu.h>
#include <lcd.h>
#include <math.h>
#include <plic.h>
#include <region_layer.h>
#include <sleep.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <uarths.h>
#include <unistd.h>
#include <utils.h>
#include <w25qxx.h>

#include "flash-manager.h"

#define PLL0_OUTPUT_FREQ 800000000UL
#define PLL1_OUTPUT_FREQ 400000000UL

#define CLASS_NUMBER 2

uint8_t model[KMODEL_SIZE];

typedef struct {
  float x;
  float y;
  float w;
  float h;
} box_t;

kpu_model_context_t task;
static region_layer_t detect_rl0, detect_rl1;

volatile uint8_t g_ai_done_flag;
static image_t kpu_image;
static int ai_done(void *ctx) {
  g_ai_done_flag = 1;
  return 0;
}

uint32_t g_lcd_gram0[320 * 224] __attribute__((aligned(16)));
uint32_t g_lcd_gram1[320 * 224] __attribute__((aligned(16)));

uint8_t g_ai_buf[320 * 224 * 3] __attribute__((
    aligned(16)));  //不知道什么原因这里必须大于224*224，太大了会闪屏

#define ANCHOR_NUM 5

float g_anchor[ANCHOR_NUM * 2] = {
    7.05,   6.88,  10.830188679245284, 17.14, 23.35, 24.00, 34.29, 55.79,
    106.87, 121.37};

// float g_anchor[ANCHOR_NUM * 2] = {31.82, 14.63, 54.4, 24.71, 87.13,
//                                   41.24, 133.0, 64.9, 185.6, 98.93};

volatile uint8_t g_dvp_finish_flag = 0;
volatile uint8_t g_ram_mux = 0;

static int on_irq_dvp(void *ctx) {
  if (dvp_get_interrupt(DVP_STS_FRAME_FINISH)) {
    /* switch gram */
    dvp_set_display_addr(g_ram_mux ? (uint32_t)g_lcd_gram0
                                   : (uint32_t)g_lcd_gram1);

    dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
    g_dvp_finish_flag = 1;
  } else {
    if (g_dvp_finish_flag == 0) dvp_start_convert();
    dvp_clear_interrupt(DVP_STS_FRAME_START);
  }

  return 0;
}

#if (CLASS_NUMBER > 1)
typedef struct {
  char *str;
  uint16_t color;
  uint16_t height;
  uint16_t width;
  uint32_t *ptr;
} class_lable_t;

class_lable_t class_lable[CLASS_NUMBER] = {{"smoking", RED}};  //{"mask", RED},

static uint32_t lable_string_draw_ram[115 * 16 * 8 / 2];
#endif

static void lable_init(void) {
#if (CLASS_NUMBER > 1)
  uint8_t index;

  class_lable[0].height = 16;
  class_lable[0].width = 8 * strlen(class_lable[0].str);
  class_lable[0].ptr = lable_string_draw_ram;
  lcd_ram_draw_string(class_lable[0].str, class_lable[0].ptr, BLACK,
                      class_lable[0].color);
  for (index = 1; index < CLASS_NUMBER; index++) {
    class_lable[index].height = 16;
    class_lable[index].width = 8 * strlen(class_lable[index].str);
    class_lable[index].ptr =
        class_lable[index - 1].ptr +
        class_lable[index - 1].height * class_lable[index - 1].width / 2;
    lcd_ram_draw_string(class_lable[index].str, class_lable[index].ptr, BLACK,
                        class_lable[index].color);
  }
#endif
}

static int max_index(float *a, int n) {
  int i, max_i = 0;
  float max = a[0];

  for (i = 1; i < n; ++i) {
    if (a[i] > max) {
      max = a[i];
      max_i = i;
    }
  }
  return max_i;
}

uint32_t max(uint32_t a, uint32_t b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

uint32_t min(uint32_t a, uint32_t b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

static void drawboxes(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                      uint32_t class, float prob) {
  //   if (x1 >= 320) x1 = 319;
  //   if (x2 >= 320) x2 = 319;
  //   if (y1 >= 240) y1 = 239;
  //   if (y2 >= 240) y2 = 239;
  if (x1 >= 320) x1 = 319;
  if (x2 >= 320) x2 = 319;
  if (y1 >= 224) y1 = 223;
  if (y2 >= 224) y2 = 223;

#if (CLASS_NUMBER > 1)
  printf("%d %d %d %d %s %f\n", x1, y1, x2, y2, class_lable[class].str, prob);
  // if (class_lable[class].str == "face") {
  //   gpio_set_pin(3, GPIO_PV_LOW);
  // }
  lcd_draw_rectangle(x1, y1, x2, y2, 2, class_lable[class].color);
  lcd_draw_picture(x1 + 1, y1 + 1, class_lable[class].width,
                   class_lable[class].height, class_lable[class].ptr);
#else
  lcd_draw_rectangle(x1, y1, x2, y2, 2, RED);
#endif
}

int main(void) {
  /* Set CPU and dvp clk */
  sysctl_pll_set_freq(SYSCTL_PLL0, PLL0_OUTPUT_FREQ);
  sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
  sysctl_set_spi0_dvp_data(1);

  // fpioa_set_function(24, FUNC_GPIO3);
  // gpio_set_drive_mode(3, GPIO_DM_OUTPUT);

  uarths_init();
  plic_init();

  printf("flash init\n");
  w25qxx_init(3, 0, 60000000);
  w25qxx_read_data(KMODEL_START, model, KMODEL_SIZE);

  lable_init();

  /* LCD init */
  printf("LCD init\n");
  lcd_init();
#if BOARD_LICHEEDAN
  lcd_set_direction(DIR_YX_RLDU);
#else
  lcd_set_direction(DIR_YX_RLUD);
#endif
  lcd_clear(BLACK);
  lcd_draw_string(235, 40, "face mask", RED);
  lcd_draw_string(235, 85, "detection", WHITE);
  lcd_draw_string(225, 130, "by xinyuuliu", GREEN);
  lcd_draw_string(0, 225, "Smoking Detection on K210 used Yolo", WHITE);
  sleep(1);
  printf("DVP init\n");
  camera_init();

  dvp_set_ai_addr((uint32_t)g_ai_buf, (uint32_t)(g_ai_buf + 320 * 224),
                  (uint32_t)(g_ai_buf + 320 * 224 * 2));
  dvp_set_display_addr((uint32_t)g_lcd_gram0);
  dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
  dvp_disable_auto();

  /* DVP interrupt config */
  printf("DVP interrupt config\n");
  plic_set_priority(IRQN_DVP_INTERRUPT, 1);
  plic_irq_register(IRQN_DVP_INTERRUPT, on_irq_dvp, NULL);
  plic_irq_enable(IRQN_DVP_INTERRUPT);

  /* enable global interrupt */
  sysctl_enable_irq();

  /* system start */
  printf("system start\n");
  g_ram_mux = 0;
  g_dvp_finish_flag = 0;
  dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
  dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);

  /* init kpu */
  if (kpu_load_kmodel(&task, model) != 0) {
    printf("\nmodel init error\n");
    while (1)
      ;
  }

  // 一共两个检测头，第0个头被关掉了，因为发现它没有检测到任何框，而且同时开两个头，屏幕会跳动
  detect_rl0.anchor_number = ANCHOR_NUM;
  detect_rl0.anchor = g_anchor;
  detect_rl0.threshold = 0.35;
  detect_rl0.nms_value = 0.1;
  region_layer_init(&detect_rl0, 10, 7, 30, 320, 224);

  while (1) {
    // gpio_set_pin(3, GPIO_PV_HIGH);
    /* dvp finish*/
    while (g_dvp_finish_flag == 0)
      ;

    /* start to calculate */
    kpu_run_kmodel(&task, g_ai_buf, DMAC_CHANNEL5, ai_done, NULL);

    while (!g_ai_done_flag)
      ;
    g_ai_done_flag = 0;

    float *output0, *output1;
    size_t output_size;
    kpu_get_output(&task, 0, &output0, &output_size);
    detect_rl0.input = output0;
    // kpu_get_output(&task, 1, &output1, &output_size);
    // detect_rl1.input = output1;

    /* start region layer */
    region_layer_run(&detect_rl0, NULL);
    // region_layer_run(&detect_rl1, NULL);

    /* display pic*/
    // region_layer_draw_boxes(&detect_rl0, drawboxes);
    g_ram_mux ^= 0x01;
    lcd_draw_picture(0, 0, 320, 224, g_ram_mux ? g_lcd_gram0 : g_lcd_gram1);

    /* draw boxs */
    // Multi_head_filter_draw_boxes(&detect_rl0, &detect_rl1, drawboxes);
    region_layer_draw_boxes(&detect_rl0, drawboxes);

    g_dvp_finish_flag = 0;
  }

  return 0;
}
