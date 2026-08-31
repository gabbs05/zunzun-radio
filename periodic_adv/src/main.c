/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>

/* Trama del enlace ZUNZUN
 *   0-1  0xFFFF   identificador de fabricante (obligatorio en el AD)
 *   2-3  seq      contador de 16 bits, +1 por trama, little endian
 *   4    idioma   0=en 1=fr 2=de 3=pt
 *   5    reservado
 *   6+   carga
 * La secuencia va en 16 bits a proposito: con 8 bits, a 50 tramas/s el
 * contador da la vuelta cada 5 s y no se distingue "perdi 1" de
 * "perdi 257".
 */
#define REFRESCO_MS 20           /* = intervalo de anuncio */

static uint8_t mfg_data[250] = { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 };
static uint16_t seq;

static const struct bt_data per_adv_ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

int main(void)
{
	struct bt_le_ext_adv *adv;
	int err;

	printk("Starting Periodic Advertising Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	/* Create a non-connectable advertising set */
	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, NULL, &adv);
	if (err) {
		printk("Failed to create advertising set (err %d)\n", err);
		return 0;
	}

	/* Set advertising data to have complete local name set */
	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Failed to set advertising data (err %d)\n", err);
		return 0;
	}

	/* Set periodic advertising parameters */
	err = bt_le_per_adv_set_param(adv,
		/* Intervalo FIJO, no un rango: con MIN/MAX el controlador
		 * elegia 0x0078 (150 ms) y el refresco de 141 ms pisaba
		 * tramas antes de emitirlas. 0x0078 = 120 * 1.25 = 150 ms.
		 */
		BT_LE_PER_ADV_PARAM(0x0010, 0x0010,
				   BT_LE_PER_ADV_OPT_NONE));
	if (err) {
		printk("Failed to set periodic advertising parameters"
		       " (err %d)\n", err);
		return 0;
	}

	/* Enable Periodic Advertising */
	err = bt_le_per_adv_start(adv);
	if (err) {
		printk("Failed to enable periodic advertising (err %d)\n", err);
		return 0;
	}

	while (true) {
		printk("Start Extended Advertising...");
		err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
		if (err) {
			printk("Failed to start extended advertising "
			       "(err %d)\n", err);
			return 0;
		}
		printk("done.\n");

		/* Antes esto era el bucle del sample de Nordic: subia un
		 * contador cada 10 s solo para demostrar que los datos se
		 * pueden cambiar en caliente. Con eso no se puede medir
		 * perdida de paquetes. Ahora se refresca a ritmo de trama.
		 */
		while (true) {
			k_sleep(K_MSEC(REFRESCO_MS));
			seq++;
			mfg_data[2] = (uint8_t)(seq & 0xff);
			mfg_data[3] = (uint8_t)(seq >> 8);
			err = bt_le_per_adv_set_data(adv, per_adv_ad,
						     ARRAY_SIZE(per_adv_ad));
			if (err) {
				printk("set_data fallo (err %d) en seq %u\n",
				       err, seq);
				return 0;
			}
		}

		k_sleep(K_SECONDS(10));

		printk("Stop Extended Advertising...");
		err = bt_le_ext_adv_stop(adv);
		if (err) {
			printk("Failed to stop extended advertising "
			       "(err %d)\n", err);
			return 0;
		}
		printk("done.\n");

		k_sleep(K_SECONDS(10));
	}
	return 0;
}
