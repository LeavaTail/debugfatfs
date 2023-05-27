// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2023 LeavaTail
 */
#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <CUnit/Console.h>
#include <CUnit/TestDB.h>
#include <stdint.h>
#include "nls.h"

void utf8_to_utf16_test_1(void)
{
	char *src = "A";
	uint16_t dist = 0;

	utf8s_to_utf16s(src, 1, &dist);
	CU_ASSERT_EQUAL(dist, 0x41);

	return;
}

void utf8_to_utf16_test_2(void)
{
	char *src = "¼";
	uint16_t dist = 0;

	utf8s_to_utf16s(src, 2, &dist);
	CU_ASSERT_EQUAL(dist, 0xBC);

	return;
}

void utf8_to_utf16_test_3(void)
{
	char *src = "あ";
	uint16_t dist = 0;

	utf8s_to_utf16s(src, 3, &dist);
	CU_ASSERT_EQUAL(dist, 0x3042);

	return;
}

void utf8_to_utf16_test_4(void)
{
	char *src = "Ō";
	uint16_t dist = 0;

	utf8s_to_utf16s(src, 2, &dist);
	CU_ASSERT_EQUAL(dist, 0x014C);

	return;
}

void utf8_to_utf16_test_5(void)
{
	char *src = "𠮷";
	uint16_t dist[2] = {0};

	utf8s_to_utf16s(src, 4, dist);
	CU_ASSERT_EQUAL(dist[0], 0xD842);
	CU_ASSERT_EQUAL(dist[1], 0xDFB7);

	return;
}

int main(void) {
	int ret;
	CU_pSuite suite;

	CU_initialize_registry();

	suite = CU_add_suite("NLS Test", NULL, NULL);
	CU_add_test(suite, "NLS_Test_1", utf8_to_utf16_test_1);
	CU_add_test(suite, "NLS_Test_2", utf8_to_utf16_test_2);
	CU_add_test(suite, "NLS_Test_3", utf8_to_utf16_test_3);
	CU_add_test(suite, "NLS_Test_4", utf8_to_utf16_test_4);
	CU_add_test(suite, "NLS_Test_5", utf8_to_utf16_test_5);

	CU_basic_run_tests();
	ret = CU_get_number_of_failures();
	CU_cleanup_registry();

	return ret;
}
