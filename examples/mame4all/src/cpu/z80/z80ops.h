/**********************************************************
 * opcodes with CB prefix
 * rotate, shift and bit operations
 **********************************************************/
OP(cb,00) { _B = RLC(_B);											} /* RLC  B 		  */
OP(cb,01) { _C = RLC(_C);											} /* RLC  C 		  */
OP(cb,02) { _D = RLC(_D);											} /* RLC  D 		  */
OP(cb,03) { _E = RLC(_E);											} /* RLC  E 		  */
OP(cb,04) { _H = RLC(_H);											} /* RLC  H 		  */
OP(cb,05) { _L = RLC(_L);											} /* RLC  L 		  */
OP(cb,06) { WM( _HL, RLC(RM(_HL)) );								} /* RLC  (HL)		  */
OP(cb,07) { _A = RLC(_A);											} /* RLC  A 		  */

OP(cb,08) { _B = RRC(_B);											} /* RRC  B 		  */
OP(cb,09) { _C = RRC(_C);											} /* RRC  C 		  */
OP(cb,0a) { _D = RRC(_D);											} /* RRC  D 		  */
OP(cb,0b) { _E = RRC(_E);											} /* RRC  E 		  */
OP(cb,0c) { _H = RRC(_H);											} /* RRC  H 		  */
OP(cb,0d) { _L = RRC(_L);											} /* RRC  L 		  */
OP(cb,0e) { WM( _HL, RRC(RM(_HL)) );								} /* RRC  (HL)		  */
OP(cb,0f) { _A = RRC(_A);											} /* RRC  A 		  */

OP(cb,10) { _B = RL(_B);											} /* RL   B 		  */
OP(cb,11) { _C = RL(_C);											} /* RL   C 		  */
OP(cb,12) { _D = RL(_D);											} /* RL   D 		  */
OP(cb,13) { _E = RL(_E);											} /* RL   E 		  */
OP(cb,14) { _H = RL(_H);											} /* RL   H 		  */
OP(cb,15) { _L = RL(_L);											} /* RL   L 		  */
OP(cb,16) { WM( _HL, RL(RM(_HL)) ); 								} /* RL   (HL)		  */
OP(cb,17) { _A = RL(_A);											} /* RL   A 		  */

OP(cb,18) { _B = RR(_B);											} /* RR   B 		  */
OP(cb,19) { _C = RR(_C);											} /* RR   C 		  */
OP(cb,1a) { _D = RR(_D);											} /* RR   D 		  */
OP(cb,1b) { _E = RR(_E);											} /* RR   E 		  */
OP(cb,1c) { _H = RR(_H);											} /* RR   H 		  */
OP(cb,1d) { _L = RR(_L);											} /* RR   L 		  */
OP(cb,1e) { WM( _HL, RR(RM(_HL)) ); 								} /* RR   (HL)		  */
OP(cb,1f) { _A = RR(_A);											} /* RR   A 		  */

OP(cb,20) { _B = SLA(_B);											} /* SLA  B 		  */
OP(cb,21) { _C = SLA(_C);											} /* SLA  C 		  */
OP(cb,22) { _D = SLA(_D);											} /* SLA  D 		  */
OP(cb,23) { _E = SLA(_E);											} /* SLA  E 		  */
OP(cb,24) { _H = SLA(_H);											} /* SLA  H 		  */
OP(cb,25) { _L = SLA(_L);											} /* SLA  L 		  */
OP(cb,26) { WM( _HL, SLA(RM(_HL)) );								} /* SLA  (HL)		  */
OP(cb,27) { _A = SLA(_A);											} /* SLA  A 		  */

OP(cb,28) { _B = SRA(_B);											} /* SRA  B 		  */
OP(cb,29) { _C = SRA(_C);											} /* SRA  C 		  */
OP(cb,2a) { _D = SRA(_D);											} /* SRA  D 		  */
OP(cb,2b) { _E = SRA(_E);											} /* SRA  E 		  */
OP(cb,2c) { _H = SRA(_H);											} /* SRA  H 		  */
OP(cb,2d) { _L = SRA(_L);											} /* SRA  L 		  */
OP(cb,2e) { WM( _HL, SRA(RM(_HL)) );								} /* SRA  (HL)		  */
OP(cb,2f) { _A = SRA(_A);											} /* SRA  A 		  */

OP(cb,30) { _B = SLL(_B);											} /* SLL  B 		  */
OP(cb,31) { _C = SLL(_C);											} /* SLL  C 		  */
OP(cb,32) { _D = SLL(_D);											} /* SLL  D 		  */
OP(cb,33) { _E = SLL(_E);											} /* SLL  E 		  */
OP(cb,34) { _H = SLL(_H);											} /* SLL  H 		  */
OP(cb,35) { _L = SLL(_L);											} /* SLL  L 		  */
OP(cb,36) { WM( _HL, SLL(RM(_HL)) );								} /* SLL  (HL)		  */
OP(cb,37) { _A = SLL(_A);											} /* SLL  A 		  */

OP(cb,38) { _B = SRL(_B);											} /* SRL  B 		  */
OP(cb,39) { _C = SRL(_C);											} /* SRL  C 		  */
OP(cb,3a) { _D = SRL(_D);											} /* SRL  D 		  */
OP(cb,3b) { _E = SRL(_E);											} /* SRL  E 		  */
OP(cb,3c) { _H = SRL(_H);											} /* SRL  H 		  */
OP(cb,3d) { _L = SRL(_L);											} /* SRL  L 		  */
OP(cb,3e) { WM( _HL, SRL(RM(_HL)) );								} /* SRL  (HL)		  */
OP(cb,3f) { _A = SRL(_A);											} /* SRL  A 		  */

OP(cb,40) { BIT(0,_B);												} /* BIT  0,B		  */
OP(cb,41) { BIT(0,_C);												} /* BIT  0,C		  */
OP(cb,42) { BIT(0,_D);												} /* BIT  0,D		  */
OP(cb,43) { BIT(0,_E);												} /* BIT  0,E		  */
OP(cb,44) { BIT(0,_H);												} /* BIT  0,H		  */
OP(cb,45) { BIT(0,_L);												} /* BIT  0,L		  */
OP(cb,46) { BIT(0,RM(_HL)); 										} /* BIT  0,(HL)	  */
OP(cb,47) { BIT(0,_A);												} /* BIT  0,A		  */

OP(cb,48) { BIT(1,_B);												} /* BIT  1,B		  */
OP(cb,49) { BIT(1,_C);												} /* BIT  1,C		  */
OP(cb,4a) { BIT(1,_D);												} /* BIT  1,D		  */
OP(cb,4b) { BIT(1,_E);												} /* BIT  1,E		  */
OP(cb,4c) { BIT(1,_H);												} /* BIT  1,H		  */
OP(cb,4d) { BIT(1,_L);												} /* BIT  1,L		  */
OP(cb,4e) { BIT(1,RM(_HL)); 										} /* BIT  1,(HL)	  */
OP(cb,4f) { BIT(1,_A);												} /* BIT  1,A		  */

OP(cb,50) { BIT(2,_B);												} /* BIT  2,B		  */
OP(cb,51) { BIT(2,_C);												} /* BIT  2,C		  */
OP(cb,52) { BIT(2,_D);												} /* BIT  2,D		  */
OP(cb,53) { BIT(2,_E);												} /* BIT  2,E		  */
OP(cb,54) { BIT(2,_H);												} /* BIT  2,H		  */
OP(cb,55) { BIT(2,_L);												} /* BIT  2,L		  */
OP(cb,56) { BIT(2,RM(_HL)); 										} /* BIT  2,(HL)	  */
OP(cb,57) { BIT(2,_A);												} /* BIT  2,A		  */

OP(cb,58) { BIT(3,_B);												} /* BIT  3,B		  */
OP(cb,59) { BIT(3,_C);												} /* BIT  3,C		  */
OP(cb,5a) { BIT(3,_D);												} /* BIT  3,D		  */
OP(cb,5b) { BIT(3,_E);												} /* BIT  3,E		  */
OP(cb,5c) { BIT(3,_H);												} /* BIT  3,H		  */
OP(cb,5d) { BIT(3,_L);												} /* BIT  3,L		  */
OP(cb,5e) { BIT(3,RM(_HL)); 										} /* BIT  3,(HL)	  */
OP(cb,5f) { BIT(3,_A);												} /* BIT  3,A		  */

OP(cb,60) { BIT(4,_B);												} /* BIT  4,B		  */
OP(cb,61) { BIT(4,_C);												} /* BIT  4,C		  */
OP(cb,62) { BIT(4,_D);												} /* BIT  4,D		  */
OP(cb,63) { BIT(4,_E);												} /* BIT  4,E		  */
OP(cb,64) { BIT(4,_H);												} /* BIT  4,H		  */
OP(cb,65) { BIT(4,_L);												} /* BIT  4,L		  */
OP(cb,66) { BIT(4,RM(_HL)); 										} /* BIT  4,(HL)	  */
OP(cb,67) { BIT(4,_A);												} /* BIT  4,A		  */

OP(cb,68) { BIT(5,_B);												} /* BIT  5,B		  */
OP(cb,69) { BIT(5,_C);												} /* BIT  5,C		  */
OP(cb,6a) { BIT(5,_D);												} /* BIT  5,D		  */
OP(cb,6b) { BIT(5,_E);												} /* BIT  5,E		  */
OP(cb,6c) { BIT(5,_H);												} /* BIT  5,H		  */
OP(cb,6d) { BIT(5,_L);												} /* BIT  5,L		  */
OP(cb,6e) { BIT(5,RM(_HL)); 										} /* BIT  5,(HL)	  */
OP(cb,6f) { BIT(5,_A);												} /* BIT  5,A		  */

OP(cb,70) { BIT(6,_B);												} /* BIT  6,B		  */
OP(cb,71) { BIT(6,_C);												} /* BIT  6,C		  */
OP(cb,72) { BIT(6,_D);												} /* BIT  6,D		  */
OP(cb,73) { BIT(6,_E);												} /* BIT  6,E		  */
OP(cb,74) { BIT(6,_H);												} /* BIT  6,H		  */
OP(cb,75) { BIT(6,_L);												} /* BIT  6,L		  */
OP(cb,76) { BIT(6,RM(_HL)); 										} /* BIT  6,(HL)	  */
OP(cb,77) { BIT(6,_A);												} /* BIT  6,A		  */

OP(cb,78) { BIT(7,_B);												} /* BIT  7,B		  */
OP(cb,79) { BIT(7,_C);												} /* BIT  7,C		  */
OP(cb,7a) { BIT(7,_D);												} /* BIT  7,D		  */
OP(cb,7b) { BIT(7,_E);												} /* BIT  7,E		  */
OP(cb,7c) { BIT(7,_H);												} /* BIT  7,H		  */
OP(cb,7d) { BIT(7,_L);												} /* BIT  7,L		  */
OP(cb,7e) { BIT(7,RM(_HL)); 										} /* BIT  7,(HL)	  */
OP(cb,7f) { BIT(7,_A);												} /* BIT  7,A		  */

OP(cb,80) { _B = RES(0,_B); 										} /* RES  0,B		  */
OP(cb,81) { _C = RES(0,_C); 										} /* RES  0,C		  */
OP(cb,82) { _D = RES(0,_D); 										} /* RES  0,D		  */
OP(cb,83) { _E = RES(0,_E); 										} /* RES  0,E		  */
OP(cb,84) { _H = RES(0,_H); 										} /* RES  0,H		  */
OP(cb,85) { _L = RES(0,_L); 										} /* RES  0,L		  */
OP(cb,86) { WM( _HL, RES(0,RM(_HL)) );								} /* RES  0,(HL)	  */
OP(cb,87) { _A = RES(0,_A); 										} /* RES  0,A		  */

OP(cb,88) { _B = RES(1,_B); 										} /* RES  1,B		  */
OP(cb,89) { _C = RES(1,_C); 										} /* RES  1,C		  */
OP(cb,8a) { _D = RES(1,_D); 										} /* RES  1,D		  */
OP(cb,8b) { _E = RES(1,_E); 										} /* RES  1,E		  */
OP(cb,8c) { _H = RES(1,_H); 										} /* RES  1,H		  */
OP(cb,8d) { _L = RES(1,_L); 										} /* RES  1,L		  */
OP(cb,8e) { WM( _HL, RES(1,RM(_HL)) );								} /* RES  1,(HL)	  */
OP(cb,8f) { _A = RES(1,_A); 										} /* RES  1,A		  */

OP(cb,90) { _B = RES(2,_B); 										} /* RES  2,B		  */
OP(cb,91) { _C = RES(2,_C); 										} /* RES  2,C		  */
OP(cb,92) { _D = RES(2,_D); 										} /* RES  2,D		  */
OP(cb,93) { _E = RES(2,_E); 										} /* RES  2,E		  */
OP(cb,94) { _H = RES(2,_H); 										} /* RES  2,H		  */
OP(cb,95) { _L = RES(2,_L); 										} /* RES  2,L		  */
OP(cb,96) { WM( _HL, RES(2,RM(_HL)) );								} /* RES  2,(HL)	  */
OP(cb,97) { _A = RES(2,_A); 										} /* RES  2,A		  */

OP(cb,98) { _B = RES(3,_B); 										} /* RES  3,B		  */
OP(cb,99) { _C = RES(3,_C); 										} /* RES  3,C		  */
OP(cb,9a) { _D = RES(3,_D); 										} /* RES  3,D		  */
OP(cb,9b) { _E = RES(3,_E); 										} /* RES  3,E		  */
OP(cb,9c) { _H = RES(3,_H); 										} /* RES  3,H		  */
OP(cb,9d) { _L = RES(3,_L); 										} /* RES  3,L		  */
OP(cb,9e) { WM( _HL, RES(3,RM(_HL)) );								} /* RES  3,(HL)	  */
OP(cb,9f) { _A = RES(3,_A); 										} /* RES  3,A		  */

OP(cb,a0) { _B = RES(4,_B); 										} /* RES  4,B		  */
OP(cb,a1) { _C = RES(4,_C); 										} /* RES  4,C		  */
OP(cb,a2) { _D = RES(4,_D); 										} /* RES  4,D		  */
OP(cb,a3) { _E = RES(4,_E); 										} /* RES  4,E		  */
OP(cb,a4) { _H = RES(4,_H); 										} /* RES  4,H		  */
OP(cb,a5) { _L = RES(4,_L); 										} /* RES  4,L		  */
OP(cb,a6) { WM( _HL, RES(4,RM(_HL)) );								} /* RES  4,(HL)	  */
OP(cb,a7) { _A = RES(4,_A); 										} /* RES  4,A		  */

OP(cb,a8) { _B = RES(5,_B); 										} /* RES  5,B		  */
OP(cb,a9) { _C = RES(5,_C); 										} /* RES  5,C		  */
OP(cb,aa) { _D = RES(5,_D); 										} /* RES  5,D		  */
OP(cb,ab) { _E = RES(5,_E); 										} /* RES  5,E		  */
OP(cb,ac) { _H = RES(5,_H); 										} /* RES  5,H		  */
OP(cb,ad) { _L = RES(5,_L); 										} /* RES  5,L		  */
OP(cb,ae) { WM( _HL, RES(5,RM(_HL)) );								} /* RES  5,(HL)	  */
OP(cb,af) { _A = RES(5,_A); 										} /* RES  5,A		  */

OP(cb,b0) { _B = RES(6,_B); 										} /* RES  6,B		  */
OP(cb,b1) { _C = RES(6,_C); 										} /* RES  6,C		  */
OP(cb,b2) { _D = RES(6,_D); 										} /* RES  6,D		  */
OP(cb,b3) { _E = RES(6,_E); 										} /* RES  6,E		  */
OP(cb,b4) { _H = RES(6,_H); 										} /* RES  6,H		  */
OP(cb,b5) { _L = RES(6,_L); 										} /* RES  6,L		  */
OP(cb,b6) { WM( _HL, RES(6,RM(_HL)) );								} /* RES  6,(HL)	  */
OP(cb,b7) { _A = RES(6,_A); 										} /* RES  6,A		  */

OP(cb,b8) { _B = RES(7,_B); 										} /* RES  7,B		  */
OP(cb,b9) { _C = RES(7,_C); 										} /* RES  7,C		  */
OP(cb,ba) { _D = RES(7,_D); 										} /* RES  7,D		  */
OP(cb,bb) { _E = RES(7,_E); 										} /* RES  7,E		  */
OP(cb,bc) { _H = RES(7,_H); 										} /* RES  7,H		  */
OP(cb,bd) { _L = RES(7,_L); 										} /* RES  7,L		  */
OP(cb,be) { WM( _HL, RES(7,RM(_HL)) );								} /* RES  7,(HL)	  */
OP(cb,bf) { _A = RES(7,_A); 										} /* RES  7,A		  */

OP(cb,c0) { _B = SET(0,_B); 										} /* SET  0,B		  */
OP(cb,c1) { _C = SET(0,_C); 										} /* SET  0,C		  */
OP(cb,c2) { _D = SET(0,_D); 										} /* SET  0,D		  */
OP(cb,c3) { _E = SET(0,_E); 										} /* SET  0,E		  */
OP(cb,c4) { _H = SET(0,_H); 										} /* SET  0,H		  */
OP(cb,c5) { _L = SET(0,_L); 										} /* SET  0,L		  */
OP(cb,c6) { WM( _HL, SET(0,RM(_HL)) );								} /* SET  0,(HL)	  */
OP(cb,c7) { _A = SET(0,_A); 										} /* SET  0,A		  */

OP(cb,c8) { _B = SET(1,_B); 										} /* SET  1,B		  */
OP(cb,c9) { _C = SET(1,_C); 										} /* SET  1,C		  */
OP(cb,ca) { _D = SET(1,_D); 										} /* SET  1,D		  */
OP(cb,cb) { _E = SET(1,_E); 										} /* SET  1,E		  */
OP(cb,cc) { _H = SET(1,_H); 										} /* SET  1,H		  */
OP(cb,cd) { _L = SET(1,_L); 										} /* SET  1,L		  */
OP(cb,ce) { WM( _HL, SET(1,RM(_HL)) );								} /* SET  1,(HL)	  */
OP(cb,cf) { _A = SET(1,_A); 										} /* SET  1,A		  */

OP(cb,d0) { _B = SET(2,_B); 										} /* SET  2,B		  */
OP(cb,d1) { _C = SET(2,_C); 										} /* SET  2,C		  */
OP(cb,d2) { _D = SET(2,_D); 										} /* SET  2,D		  */
OP(cb,d3) { _E = SET(2,_E); 										} /* SET  2,E		  */
OP(cb,d4) { _H = SET(2,_H); 										} /* SET  2,H		  */
OP(cb,d5) { _L = SET(2,_L); 										} /* SET  2,L		  */
OP(cb,d6) { WM( _HL, SET(2,RM(_HL)) );								}/* SET  2,(HL) 	 */
OP(cb,d7) { _A = SET(2,_A); 										} /* SET  2,A		  */

OP(cb,d8) { _B = SET(3,_B); 										} /* SET  3,B		  */
OP(cb,d9) { _C = SET(3,_C); 										} /* SET  3,C		  */
OP(cb,da) { _D = SET(3,_D); 										} /* SET  3,D		  */
OP(cb,db) { _E = SET(3,_E); 										} /* SET  3,E		  */
OP(cb,dc) { _H = SET(3,_H); 										} /* SET  3,H		  */
OP(cb,dd) { _L = SET(3,_L); 										} /* SET  3,L		  */
OP(cb,de) { WM( _HL, SET(3,RM(_HL)) );								} /* SET  3,(HL)	  */
OP(cb,df) { _A = SET(3,_A); 										} /* SET  3,A		  */

OP(cb,e0) { _B = SET(4,_B); 										} /* SET  4,B		  */
OP(cb,e1) { _C = SET(4,_C); 										} /* SET  4,C		  */
OP(cb,e2) { _D = SET(4,_D); 										} /* SET  4,D		  */
OP(cb,e3) { _E = SET(4,_E); 										} /* SET  4,E		  */
OP(cb,e4) { _H = SET(4,_H); 										} /* SET  4,H		  */
OP(cb,e5) { _L = SET(4,_L); 										} /* SET  4,L		  */
OP(cb,e6) { WM( _HL, SET(4,RM(_HL)) );								} /* SET  4,(HL)	  */
OP(cb,e7) { _A = SET(4,_A); 										} /* SET  4,A		  */

OP(cb,e8) { _B = SET(5,_B); 										} /* SET  5,B		  */
OP(cb,e9) { _C = SET(5,_C); 										} /* SET  5,C		  */
OP(cb,ea) { _D = SET(5,_D); 										} /* SET  5,D		  */
OP(cb,eb) { _E = SET(5,_E); 										} /* SET  5,E		  */
OP(cb,ec) { _H = SET(5,_H); 										} /* SET  5,H		  */
OP(cb,ed) { _L = SET(5,_L); 										} /* SET  5,L		  */
OP(cb,ee) { WM( _HL, SET(5,RM(_HL)) );								} /* SET  5,(HL)	  */
OP(cb,ef) { _A = SET(5,_A); 										} /* SET  5,A		  */

OP(cb,f0) { _B = SET(6,_B); 										} /* SET  6,B		  */
OP(cb,f1) { _C = SET(6,_C); 										} /* SET  6,C		  */
OP(cb,f2) { _D = SET(6,_D); 										} /* SET  6,D		  */
OP(cb,f3) { _E = SET(6,_E); 										} /* SET  6,E		  */
OP(cb,f4) { _H = SET(6,_H); 										} /* SET  6,H		  */
OP(cb,f5) { _L = SET(6,_L); 										} /* SET  6,L		  */
OP(cb,f6) { WM( _HL, SET(6,RM(_HL)) );								} /* SET  6,(HL)	  */
OP(cb,f7) { _A = SET(6,_A); 										} /* SET  6,A		  */

OP(cb,f8) { _B = SET(7,_B); 										} /* SET  7,B		  */
OP(cb,f9) { _C = SET(7,_C); 										} /* SET  7,C		  */
OP(cb,fa) { _D = SET(7,_D); 										} /* SET  7,D		  */
OP(cb,fb) { _E = SET(7,_E); 										} /* SET  7,E		  */
OP(cb,fc) { _H = SET(7,_H); 										} /* SET  7,H		  */
OP(cb,fd) { _L = SET(7,_L); 										} /* SET  7,L		  */
OP(cb,fe) { WM( _HL, SET(7,RM(_HL)) );								} /* SET  7,(HL)	  */
OP(cb,ff) { _A = SET(7,_A); 										} /* SET  7,A		  */


/**********************************************************
* opcodes with DD/FD CB prefix
* rotate, shift and bit operations with (IX+o)
**********************************************************/
OP(xycb,00) { _B = RLC( RM(EA) ); WM( EA,_B );						} /* RLC  B=(XY+o)	  */
OP(xycb,01) { _C = RLC( RM(EA) ); WM( EA,_C );						} /* RLC  C=(XY+o)	  */
OP(xycb,02) { _D = RLC( RM(EA) ); WM( EA,_D );						} /* RLC  D=(XY+o)	  */
OP(xycb,03) { _E = RLC( RM(EA) ); WM( EA,_E );						} /* RLC  E=(XY+o)	  */
OP(xycb,04) { _H = RLC( RM(EA) ); WM( EA,_H );						} /* RLC  H=(XY+o)	  */
OP(xycb,05) { _L = RLC( RM(EA) ); WM( EA,_L );						} /* RLC  L=(XY+o)	  */
OP(xycb,06) { WM( EA, RLC( RM(EA) ) );								} /* RLC  (XY+o)	  */
OP(xycb,07) { _A = RLC( RM(EA) ); WM( EA,_A );						} /* RLC  A=(XY+o)	  */

OP(xycb,08) { _B = RRC( RM(EA) ); WM( EA,_B );						} /* RRC  B=(XY+o)	  */
OP(xycb,09) { _C = RRC( RM(EA) ); WM( EA,_C );						} /* RRC  C=(XY+o)	  */
OP(xycb,0a) { _D = RRC( RM(EA) ); WM( EA,_D );						} /* RRC  D=(XY+o)	  */
OP(xycb,0b) { _E = RRC( RM(EA) ); WM( EA,_E );						} /* RRC  E=(XY+o)	  */
OP(xycb,0c) { _H = RRC( RM(EA) ); WM( EA,_H );						} /* RRC  H=(XY+o)	  */
OP(xycb,0d) { _L = RRC( RM(EA) ); WM( EA,_L );						} /* RRC  L=(XY+o)	  */
OP(xycb,0e) { WM( EA,RRC( RM(EA) ) );								} /* RRC  (XY+o)	  */
OP(xycb,0f) { _A = RRC( RM(EA) ); WM( EA,_A );						} /* RRC  A=(XY+o)	  */

OP(xycb,10) { _B = RL( RM(EA) ); WM( EA,_B );						} /* RL   B=(XY+o)	  */
OP(xycb,11) { _C = RL( RM(EA) ); WM( EA,_C );						} /* RL   C=(XY+o)	  */
OP(xycb,12) { _D = RL( RM(EA) ); WM( EA,_D );						} /* RL   D=(XY+o)	  */
OP(xycb,13) { _E = RL( RM(EA) ); WM( EA,_E );						} /* RL   E=(XY+o)	  */
OP(xycb,14) { _H = RL( RM(EA) ); WM( EA,_H );						} /* RL   H=(XY+o)	  */
OP(xycb,15) { _L = RL( RM(EA) ); WM( EA,_L );						} /* RL   L=(XY+o)	  */
OP(xycb,16) { WM( EA,RL( RM(EA) ) );								} /* RL   (XY+o)	  */
OP(xycb,17) { _A = RL( RM(EA) ); WM( EA,_A );						} /* RL   A=(XY+o)	  */

OP(xycb,18) { _B = RR( RM(EA) ); WM( EA,_B );						} /* RR   B=(XY+o)	  */
OP(xycb,19) { _C = RR( RM(EA) ); WM( EA,_C );						} /* RR   C=(XY+o)	  */
OP(xycb,1a) { _D = RR( RM(EA) ); WM( EA,_D );						} /* RR   D=(XY+o)	  */
OP(xycb,1b) { _E = RR( RM(EA) ); WM( EA,_E );						} /* RR   E=(XY+o)	  */
OP(xycb,1c) { _H = RR( RM(EA) ); WM( EA,_H );						} /* RR   H=(XY+o)	  */
OP(xycb,1d) { _L = RR( RM(EA) ); WM( EA,_L );						} /* RR   L=(XY+o)	  */
OP(xycb,1e) { WM( EA,RR( RM(EA) ) );								} /* RR   (XY+o)	  */
OP(xycb,1f) { _A = RR( RM(EA) ); WM( EA,_A );						} /* RR   A=(XY+o)	  */

OP(xycb,20) { _B = SLA( RM(EA) ); WM( EA,_B );						} /* SLA  B=(XY+o)	  */
OP(xycb,21) { _C = SLA( RM(EA) ); WM( EA,_C );						} /* SLA  C=(XY+o)	  */
OP(xycb,22) { _D = SLA( RM(EA) ); WM( EA,_D );						} /* SLA  D=(XY+o)	  */
OP(xycb,23) { _E = SLA( RM(EA) ); WM( EA,_E );						} /* SLA  E=(XY+o)	  */
OP(xycb,24) { _H = SLA( RM(EA) ); WM( EA,_H );						} /* SLA  H=(XY+o)	  */
OP(xycb,25) { _L = SLA( RM(EA) ); WM( EA,_L );						} /* SLA  L=(XY+o)	  */
OP(xycb,26) { WM( EA,SLA( RM(EA) ) );								} /* SLA  (XY+o)	  */
OP(xycb,27) { _A = SLA( RM(EA) ); WM( EA,_A );						} /* SLA  A=(XY+o)	  */

OP(xycb,28) { _B = SRA( RM(EA) ); WM( EA,_B );						} /* SRA  B=(XY+o)	  */
OP(xycb,29) { _C = SRA( RM(EA) ); WM( EA,_C );						} /* SRA  C=(XY+o)	  */
OP(xycb,2a) { _D = SRA( RM(EA) ); WM( EA,_D );						} /* SRA  D=(XY+o)	  */
OP(xycb,2b) { _E = SRA( RM(EA) ); WM( EA,_E );						} /* SRA  E=(XY+o)	  */
OP(xycb,2c) { _H = SRA( RM(EA) ); WM( EA,_H );						} /* SRA  H=(XY+o)	  */
OP(xycb,2d) { _L = SRA( RM(EA) ); WM( EA,_L );						} /* SRA  L=(XY+o)	  */
OP(xycb,2e) { WM( EA,SRA( RM(EA) ) );								} /* SRA  (XY+o)	  */
OP(xycb,2f) { _A = SRA( RM(EA) ); WM( EA,_A );						} /* SRA  A=(XY+o)	  */

OP(xycb,30) { _B = SLL( RM(EA) ); WM( EA,_B );						} /* SLL  B=(XY+o)	  */
OP(xycb,31) { _C = SLL( RM(EA) ); WM( EA,_C );						} /* SLL  C=(XY+o)	  */
OP(xycb,32) { _D = SLL( RM(EA) ); WM( EA,_D );						} /* SLL  D=(XY+o)	  */
OP(xycb,33) { _E = SLL( RM(EA) ); WM( EA,_E );						} /* SLL  E=(XY+o)	  */
OP(xycb,34) { _H = SLL( RM(EA) ); WM( EA,_H );						} /* SLL  H=(XY+o)	  */
OP(xycb,35) { _L = SLL( RM(EA) ); WM( EA,_L );						} /* SLL  L=(XY+o)	  */
OP(xycb,36) { WM( EA,SLL( RM(EA) ) );								} /* SLL  (XY+o)	  */
OP(xycb,37) { _A = SLL( RM(EA) ); WM( EA,_A );						} /* SLL  A=(XY+o)	  */

OP(xycb,38) { _B = SRL( RM(EA) ); WM( EA,_B );						} /* SRL  B=(XY+o)	  */
OP(xycb,39) { _C = SRL( RM(EA) ); WM( EA,_C );						} /* SRL  C=(XY+o)	  */
OP(xycb,3a) { _D = SRL( RM(EA) ); WM( EA,_D );						} /* SRL  D=(XY+o)	  */
OP(xycb,3b) { _E = SRL( RM(EA) ); WM( EA,_E );						} /* SRL  E=(XY+o)	  */
OP(xycb,3c) { _H = SRL( RM(EA) ); WM( EA,_H );						} /* SRL  H=(XY+o)	  */
OP(xycb,3d) { _L = SRL( RM(EA) ); WM( EA,_L );						} /* SRL  L=(XY+o)	  */
OP(xycb,3e) { WM( EA,SRL( RM(EA) ) );								} /* SRL  (XY+o)	  */
OP(xycb,3f) { _A = SRL( RM(EA) ); WM( EA,_A );						} /* SRL  A=(XY+o)	  */

OP(xycb,40) { xycb_46();											} /* BIT  0,B=(XY+o)  */
OP(xycb,41) { xycb_46();													  } /* BIT	0,C=(XY+o)	*/
OP(xycb,42) { xycb_46();											} /* BIT  0,D=(XY+o)  */
OP(xycb,43) { xycb_46();											} /* BIT  0,E=(XY+o)  */
OP(xycb,44) { xycb_46();											} /* BIT  0,H=(XY+o)  */
OP(xycb,45) { xycb_46();											} /* BIT  0,L=(XY+o)  */
OP(xycb,46) { BIT_XY(0,RM(EA)); 									} /* BIT  0,(XY+o)	  */
OP(xycb,47) { xycb_46();											} /* BIT  0,A=(XY+o)  */

OP(xycb,48) { xycb_4e();											} /* BIT  1,B=(XY+o)  */
OP(xycb,49) { xycb_4e();													  } /* BIT	1,C=(XY+o)	*/
OP(xycb,4a) { xycb_4e();											} /* BIT  1,D=(XY+o)  */
OP(xycb,4b) { xycb_4e();											} /* BIT  1,E=(XY+o)  */
OP(xycb,4c) { xycb_4e();											} /* BIT  1,H=(XY+o)  */
OP(xycb,4d) { xycb_4e();											} /* BIT  1,L=(XY+o)  */
OP(xycb,4e) { BIT_XY(1,RM(EA)); 									} /* BIT  1,(XY+o)	  */
OP(xycb,4f) { xycb_4e();											} /* BIT  1,A=(XY+o)  */

OP(xycb,50) { xycb_56();											} /* BIT  2,B=(XY+o)  */
OP(xycb,51) { xycb_56();													  } /* BIT	2,C=(XY+o)	*/
OP(xycb,52) { xycb_56();											} /* BIT  2,D=(XY+o)  */
OP(xycb,53) { xycb_56();											} /* BIT  2,E=(XY+o)  */
OP(xycb,54) { xycb_56();											} /* BIT  2,H=(XY+o)  */
OP(xycb,55) { xycb_56();											} /* BIT  2,L=(XY+o)  */
OP(xycb,56) { BIT_XY(2,RM(EA)); 									} /* BIT  2,(XY+o)	  */
OP(xycb,57) { xycb_56();											} /* BIT  2,A=(XY+o)  */

OP(xycb,58) { xycb_5e();											} /* BIT  3,B=(XY+o)  */
OP(xycb,59) { xycb_5e();													  } /* BIT	3,C=(XY+o)	*/
OP(xycb,5a) { xycb_5e();											} /* BIT  3,D=(XY+o)  */
OP(xycb,5b) { xycb_5e();											} /* BIT  3,E=(XY+o)  */
OP(xycb,5c) { xycb_5e();											} /* BIT  3,H=(XY+o)  */
OP(xycb,5d) { xycb_5e();											} /* BIT  3,L=(XY+o)  */
OP(xycb,5e) { BIT_XY(3,RM(EA)); 									} /* BIT  3,(XY+o)	  */
OP(xycb,5f) { xycb_5e();											} /* BIT  3,A=(XY+o)  */

OP(xycb,60) { xycb_66();											} /* BIT  4,B=(XY+o)  */
OP(xycb,61) { xycb_66();													  } /* BIT	4,C=(XY+o)	*/
OP(xycb,62) { xycb_66();											} /* BIT  4,D=(XY+o)  */
OP(xycb,63) { xycb_66();											} /* BIT  4,E=(XY+o)  */
OP(xycb,64) { xycb_66();											} /* BIT  4,H=(XY+o)  */
OP(xycb,65) { xycb_66();											} /* BIT  4,L=(XY+o)  */
OP(xycb,66) { BIT_XY(4,RM(EA)); 									} /* BIT  4,(XY+o)	  */
OP(xycb,67) { xycb_66();											} /* BIT  4,A=(XY+o)  */

OP(xycb,68) { xycb_6e();											} /* BIT  5,B=(XY+o)  */
OP(xycb,69) { xycb_6e();													  } /* BIT	5,C=(XY+o)	*/
OP(xycb,6a) { xycb_6e();											} /* BIT  5,D=(XY+o)  */
OP(xycb,6b) { xycb_6e();											} /* BIT  5,E=(XY+o)  */
OP(xycb,6c) { xycb_6e();											} /* BIT  5,H=(XY+o)  */
OP(xycb,6d) { xycb_6e();											} /* BIT  5,L=(XY+o)  */
OP(xycb,6e) { BIT_XY(5,RM(EA)); 									} /* BIT  5,(XY+o)	  */
OP(xycb,6f) { xycb_6e();											} /* BIT  5,A=(XY+o)  */

OP(xycb,70) { xycb_76();											} /* BIT  6,B=(XY+o)  */
OP(xycb,71) { xycb_76();													  } /* BIT	6,C=(XY+o)	*/
OP(xycb,72) { xycb_76();											} /* BIT  6,D=(XY+o)  */
OP(xycb,73) { xycb_76();											} /* BIT  6,E=(XY+o)  */
OP(xycb,74) { xycb_76();											} /* BIT  6,H=(XY+o)  */
OP(xycb,75) { xycb_76();											} /* BIT  6,L=(XY+o)  */
OP(xycb,76) { BIT_XY(6,RM(EA)); 									} /* BIT  6,(XY+o)	  */
OP(xycb,77) { xycb_76();											} /* BIT  6,A=(XY+o)  */

OP(xycb,78) { xycb_7e();											} /* BIT  7,B=(XY+o)  */
OP(xycb,79) { xycb_7e();													  } /* BIT	7,C=(XY+o)	*/
OP(xycb,7a) { xycb_7e();											} /* BIT  7,D=(XY+o)  */
OP(xycb,7b) { xycb_7e();											} /* BIT  7,E=(XY+o)  */
OP(xycb,7c) { xycb_7e();											} /* BIT  7,H=(XY+o)  */
OP(xycb,7d) { xycb_7e();											} /* BIT  7,L=(XY+o)  */
OP(xycb,7e) { BIT_XY(7,RM(EA)); 									} /* BIT  7,(XY+o)	  */
OP(xycb,7f) { xycb_7e();											} /* BIT  7,A=(XY+o)  */

OP(xycb,80) { _B = RES(0, RM(EA) ); WM( EA,_B );					} /* RES  0,B=(XY+o)  */
OP(xycb,81) { _C = RES(0, RM(EA) ); WM( EA,_C );					} /* RES  0,C=(XY+o)  */
OP(xycb,82) { _D = RES(0, RM(EA) ); WM( EA,_D );					} /* RES  0,D=(XY+o)  */
OP(xycb,83) { _E = RES(0, RM(EA) ); WM( EA,_E );					} /* RES  0,E=(XY+o)  */
OP(xycb,84) { _H = RES(0, RM(EA) ); WM( EA,_H );					} /* RES  0,H=(XY+o)  */
OP(xycb,85) { _L = RES(0, RM(EA) ); WM( EA,_L );					} /* RES  0,L=(XY+o)  */
OP(xycb,86) { WM( EA, RES(0,RM(EA)) );								} /* RES  0,(XY+o)	  */
OP(xycb,87) { _A = RES(0, RM(EA) ); WM( EA,_A );					} /* RES  0,A=(XY+o)  */

OP(xycb,88) { _B = RES(1, RM(EA) ); WM( EA,_B );					} /* RES  1,B=(XY+o)  */
OP(xycb,89) { _C = RES(1, RM(EA) ); WM( EA,_C );					} /* RES  1,C=(XY+o)  */
OP(xycb,8a) { _D = RES(1, RM(EA) ); WM( EA,_D );					} /* RES  1,D=(XY+o)  */
OP(xycb,8b) { _E = RES(1, RM(EA) ); WM( EA,_E );					} /* RES  1,E=(XY+o)  */
OP(xycb,8c) { _H = RES(1, RM(EA) ); WM( EA,_H );					} /* RES  1,H=(XY+o)  */
OP(xycb,8d) { _L = RES(1, RM(EA) ); WM( EA,_L );					} /* RES  1,L=(XY+o)  */
OP(xycb,8e) { WM( EA, RES(1,RM(EA)) );								} /* RES  1,(XY+o)	  */
OP(xycb,8f) { _A = RES(1, RM(EA) ); WM( EA,_A );					} /* RES  1,A=(XY+o)  */

OP(xycb,90) { _B = RES(2, RM(EA) ); WM( EA,_B );					} /* RES  2,B=(XY+o)  */
OP(xycb,91) { _C = RES(2, RM(EA) ); WM( EA,_C );					} /* RES  2,C=(XY+o)  */
OP(xycb,92) { _D = RES(2, RM(EA) ); WM( EA,_D );					} /* RES  2,D=(XY+o)  */
OP(xycb,93) { _E = RES(2, RM(EA) ); WM( EA,_E );					} /* RES  2,E=(XY+o)  */
OP(xycb,94) { _H = RES(2, RM(EA) ); WM( EA,_H );					} /* RES  2,H=(XY+o)  */
OP(xycb,95) { _L = RES(2, RM(EA) ); WM( EA,_L );					} /* RES  2,L=(XY+o)  */
OP(xycb,96) { WM( EA, RES(2,RM(EA)) );								} /* RES  2,(XY+o)	  */
OP(xycb,97) { _A = RES(2, RM(EA) ); WM( EA,_A );					} /* RES  2,A=(XY+o)  */

OP(xycb,98) { _B = RES(3, RM(EA) ); WM( EA,_B );					} /* RES  3,B=(XY+o)  */
OP(xycb,99) { _C = RES(3, RM(EA) ); WM( EA,_C );					} /* RES  3,C=(XY+o)  */
OP(xycb,9a) { _D = RES(3, RM(EA) ); WM( EA,_D );					} /* RES  3,D=(XY+o)  */
OP(xycb,9b) { _E = RES(3, RM(EA) ); WM( EA,_E );					} /* RES  3,E=(XY+o)  */
OP(xycb,9c) { _H = RES(3, RM(EA) ); WM( EA,_H );					} /* RES  3,H=(XY+o)  */
OP(xycb,9d) { _L = RES(3, RM(EA) ); WM( EA,_L );					} /* RES  3,L=(XY+o)  */
OP(xycb,9e) { WM( EA, RES(3,RM(EA)) );								} /* RES  3,(XY+o)	  */
OP(xycb,9f) { _A = RES(3, RM(EA) ); WM( EA,_A );					} /* RES  3,A=(XY+o)  */

OP(xycb,a0) { _B = RES(4, RM(EA) ); WM( EA,_B );					} /* RES  4,B=(XY+o)  */
OP(xycb,a1) { _C = RES(4, RM(EA) ); WM( EA,_C );					} /* RES  4,C=(XY+o)  */
OP(xycb,a2) { _D = RES(4, RM(EA) ); WM( EA,_D );					} /* RES  4,D=(XY+o)  */
OP(xycb,a3) { _E = RES(4, RM(EA) ); WM( EA,_E );					} /* RES  4,E=(XY+o)  */
OP(xycb,a4) { _H = RES(4, RM(EA) ); WM( EA,_H );					} /* RES  4,H=(XY+o)  */
OP(xycb,a5) { _L = RES(4, RM(EA) ); WM( EA,_L );					} /* RES  4,L=(XY+o)  */
OP(xycb,a6) { WM( EA, RES(4,RM(EA)) );								} /* RES  4,(XY+o)	  */
OP(xycb,a7) { _A = RES(4, RM(EA) ); WM( EA,_A );					} /* RES  4,A=(XY+o)  */

OP(xycb,a8) { _B = RES(5, RM(EA) ); WM( EA,_B );					} /* RES  5,B=(XY+o)  */
OP(xycb,a9) { _C = RES(5, RM(EA) ); WM( EA,_C );					} /* RES  5,C=(XY+o)  */
OP(xycb,aa) { _D = RES(5, RM(EA) ); WM( EA,_D );					} /* RES  5,D=(XY+o)  */
OP(xycb,ab) { _E = RES(5, RM(EA) ); WM( EA,_E );					} /* RES  5,E=(XY+o)  */
OP(xycb,ac) { _H = RES(5, RM(EA) ); WM( EA,_H );					} /* RES  5,H=(XY+o)  */
OP(xycb,ad) { _L = RES(5, RM(EA) ); WM( EA,_L );					} /* RES  5,L=(XY+o)  */
OP(xycb,ae) { WM( EA, RES(5,RM(EA)) );								} /* RES  5,(XY+o)	  */
OP(xycb,af) { _A = RES(5, RM(EA) ); WM( EA,_A );					} /* RES  5,A=(XY+o)  */

OP(xycb,b0) { _B = RES(6, RM(EA) ); WM( EA,_B );					} /* RES  6,B=(XY+o)  */
OP(xycb,b1) { _C = RES(6, RM(EA) ); WM( EA,_C );					} /* RES  6,C=(XY+o)  */
OP(xycb,b2) { _D = RES(6, RM(EA) ); WM( EA,_D );					} /* RES  6,D=(XY+o)  */
OP(xycb,b3) { _E = RES(6, RM(EA) ); WM( EA,_E );					} /* RES  6,E=(XY+o)  */
OP(xycb,b4) { _H = RES(6, RM(EA) ); WM( EA,_H );					} /* RES  6,H=(XY+o)  */
OP(xycb,b5) { _L = RES(6, RM(EA) ); WM( EA,_L );					} /* RES  6,L=(XY+o)  */
OP(xycb,b6) { WM( EA, RES(6,RM(EA)) );								} /* RES  6,(XY+o)	  */
OP(xycb,b7) { _A = RES(6, RM(EA) ); WM( EA,_A );					} /* RES  6,A=(XY+o)  */

OP(xycb,b8) { _B = RES(7, RM(EA) ); WM( EA,_B );					} /* RES  7,B=(XY+o)  */
OP(xycb,b9) { _C = RES(7, RM(EA) ); WM( EA,_C );					} /* RES  7,C=(XY+o)  */
OP(xycb,ba) { _D = RES(7, RM(EA) ); WM( EA,_D );					} /* RES  7,D=(XY+o)  */
OP(xycb,bb) { _E = RES(7, RM(EA) ); WM( EA,_E );					} /* RES  7,E=(XY+o)  */
OP(xycb,bc) { _H = RES(7, RM(EA) ); WM( EA,_H );					} /* RES  7,H=(XY+o)  */
OP(xycb,bd) { _L = RES(7, RM(EA) ); WM( EA,_L );					} /* RES  7,L=(XY+o)  */
OP(xycb,be) { WM( EA, RES(7,RM(EA)) );								} /* RES  7,(XY+o)	  */
OP(xycb,bf) { _A = RES(7, RM(EA) ); WM( EA,_A );					} /* RES  7,A=(XY+o)  */

OP(xycb,c0) { _B = SET(0, RM(EA) ); WM( EA,_B );					} /* SET  0,B=(XY+o)  */
OP(xycb,c1) { _C = SET(0, RM(EA) ); WM( EA,_C );					} /* SET  0,C=(XY+o)  */
OP(xycb,c2) { _D = SET(0, RM(EA) ); WM( EA,_D );					} /* SET  0,D=(XY+o)  */
OP(xycb,c3) { _E = SET(0, RM(EA) ); WM( EA,_E );					} /* SET  0,E=(XY+o)  */
OP(xycb,c4) { _H = SET(0, RM(EA) ); WM( EA,_H );					} /* SET  0,H=(XY+o)  */
OP(xycb,c5) { _L = SET(0, RM(EA) ); WM( EA,_L );					} /* SET  0,L=(XY+o)  */
OP(xycb,c6) { WM( EA, SET(0,RM(EA)) );								} /* SET  0,(XY+o)	  */
OP(xycb,c7) { _A = SET(0, RM(EA) ); WM( EA,_A );					} /* SET  0,A=(XY+o)  */

OP(xycb,c8) { _B = SET(1, RM(EA) ); WM( EA,_B );					} /* SET  1,B=(XY+o)  */
OP(xycb,c9) { _C = SET(1, RM(EA) ); WM( EA,_C );					} /* SET  1,C=(XY+o)  */
OP(xycb,ca) { _D = SET(1, RM(EA) ); WM( EA,_D );					} /* SET  1,D=(XY+o)  */
OP(xycb,cb) { _E = SET(1, RM(EA) ); WM( EA,_E );					} /* SET  1,E=(XY+o)  */
OP(xycb,cc) { _H = SET(1, RM(EA) ); WM( EA,_H );					} /* SET  1,H=(XY+o)  */
OP(xycb,cd) { _L = SET(1, RM(EA) ); WM( EA,_L );					} /* SET  1,L=(XY+o)  */
OP(xycb,ce) { WM( EA, SET(1,RM(EA)) );								} /* SET  1,(XY+o)	  */
OP(xycb,cf) { _A = SET(1, RM(EA) ); WM( EA,_A );					} /* SET  1,A=(XY+o)  */

OP(xycb,d0) { _B = SET(2, RM(EA) ); WM( EA,_B );					} /* SET  2,B=(XY+o)  */
OP(xycb,d1) { _C = SET(2, RM(EA) ); WM( EA,_C );					} /* SET  2,C=(XY+o)  */
OP(xycb,d2) { _D = SET(2, RM(EA) ); WM( EA,_D );					} /* SET  2,D=(XY+o)  */
OP(xycb,d3) { _E = SET(2, RM(EA) ); WM( EA,_E );					} /* SET  2,E=(XY+o)  */
OP(xycb,d4) { _H = SET(2, RM(EA) ); WM( EA,_H );					} /* SET  2,H=(XY+o)  */
OP(xycb,d5) { _L = SET(2, RM(EA) ); WM( EA,_L );					} /* SET  2,L=(XY+o)  */
OP(xycb,d6) { WM( EA, SET(2,RM(EA)) );								} /* SET  2,(XY+o)	  */
OP(xycb,d7) { _A = SET(2, RM(EA) ); WM( EA,_A );					} /* SET  2,A=(XY+o)  */

OP(xycb,d8) { _B = SET(3, RM(EA) ); WM( EA,_B );					} /* SET  3,B=(XY+o)  */
OP(xycb,d9) { _C = SET(3, RM(EA) ); WM( EA,_C );					} /* SET  3,C=(XY+o)  */
OP(xycb,da) { _D = SET(3, RM(EA) ); WM( EA,_D );					} /* SET  3,D=(XY+o)  */
OP(xycb,db) { _E = SET(3, RM(EA) ); WM( EA,_E );					} /* SET  3,E=(XY+o)  */
OP(xycb,dc) { _H = SET(3, RM(EA) ); WM( EA,_H );					} /* SET  3,H=(XY+o)  */
OP(xycb,dd) { _L = SET(3, RM(EA) ); WM( EA,_L );					} /* SET  3,L=(XY+o)  */
OP(xycb,de) { WM( EA, SET(3,RM(EA)) );								} /* SET  3,(XY+o)	  */
OP(xycb,df) { _A = SET(3, RM(EA) ); WM( EA,_A );					} /* SET  3,A=(XY+o)  */

OP(xycb,e0) { _B = SET(4, RM(EA) ); WM( EA,_B );					} /* SET  4,B=(XY+o)  */
OP(xycb,e1) { _C = SET(4, RM(EA) ); WM( EA,_C );					} /* SET  4,C=(XY+o)  */
OP(xycb,e2) { _D = SET(4, RM(EA) ); WM( EA,_D );					} /* SET  4,D=(XY+o)  */
OP(xycb,e3) { _E = SET(4, RM(EA) ); WM( EA,_E );					} /* SET  4,E=(XY+o)  */
OP(xycb,e4) { _H = SET(4, RM(EA) ); WM( EA,_H );					} /* SET  4,H=(XY+o)  */
OP(xycb,e5) { _L = SET(4, RM(EA) ); WM( EA,_L );					} /* SET  4,L=(XY+o)  */
OP(xycb,e6) { WM( EA, SET(4,RM(EA)) );								} /* SET  4,(XY+o)	  */
OP(xycb,e7) { _A = SET(4, RM(EA) ); WM( EA,_A );					} /* SET  4,A=(XY+o)  */

OP(xycb,e8) { _B = SET(5, RM(EA) ); WM( EA,_B );					} /* SET  5,B=(XY+o)  */
OP(xycb,e9) { _C = SET(5, RM(EA) ); WM( EA,_C );					} /* SET  5,C=(XY+o)  */
OP(xycb,ea) { _D = SET(5, RM(EA) ); WM( EA,_D );					} /* SET  5,D=(XY+o)  */
OP(xycb,eb) { _E = SET(5, RM(EA) ); WM( EA,_E );					} /* SET  5,E=(XY+o)  */
OP(xycb,ec) { _H = SET(5, RM(EA) ); WM( EA,_H );					} /* SET  5,H=(XY+o)  */
OP(xycb,ed) { _L = SET(5, RM(EA) ); WM( EA,_L );					} /* SET  5,L=(XY+o)  */
OP(xycb,ee) { WM( EA, SET(5,RM(EA)) );								} /* SET  5,(XY+o)	  */
OP(xycb,ef) { _A = SET(5, RM(EA) ); WM( EA,_A );					} /* SET  5,A=(XY+o)  */

OP(xycb,f0) { _B = SET(6, RM(EA) ); WM( EA,_B );					} /* SET  6,B=(XY+o)  */
OP(xycb,f1) { _C = SET(6, RM(EA) ); WM( EA,_C );					} /* SET  6,C=(XY+o)  */
OP(xycb,f2) { _D = SET(6, RM(EA) ); WM( EA,_D );					} /* SET  6,D=(XY+o)  */
OP(xycb,f3) { _E = SET(6, RM(EA) ); WM( EA,_E );					} /* SET  6,E=(XY+o)  */
OP(xycb,f4) { _H = SET(6, RM(EA) ); WM( EA,_H );					} /* SET  6,H=(XY+o)  */
OP(xycb,f5) { _L = SET(6, RM(EA) ); WM( EA,_L );					} /* SET  6,L=(XY+o)  */
OP(xycb,f6) { WM( EA, SET(6,RM(EA)) );								} /* SET  6,(XY+o)	  */
OP(xycb,f7) { _A = SET(6, RM(EA) ); WM( EA,_A );					} /* SET  6,A=(XY+o)  */

OP(xycb,f8) { _B = SET(7, RM(EA) ); WM( EA,_B );					} /* SET  7,B=(XY+o)  */
OP(xycb,f9) { _C = SET(7, RM(EA) ); WM( EA,_C );					} /* SET  7,C=(XY+o)  */
OP(xycb,fa) { _D = SET(7, RM(EA) ); WM( EA,_D );					} /* SET  7,D=(XY+o)  */
OP(xycb,fb) { _E = SET(7, RM(EA) ); WM( EA,_E );					} /* SET  7,E=(XY+o)  */
OP(xycb,fc) { _H = SET(7, RM(EA) ); WM( EA,_H );					} /* SET  7,H=(XY+o)  */
OP(xycb,fd) { _L = SET(7, RM(EA) ); WM( EA,_L );					} /* SET  7,L=(XY+o)  */
OP(xycb,fe) { WM( EA, SET(7,RM(EA)) );								} /* SET  7,(XY+o)	  */
OP(xycb,ff) { _A = SET(7, RM(EA) ); WM( EA,_A );					} /* SET  7,A=(XY+o)  */

/**********************************************************
 * IX register related opcodes (DD prefix)
 **********************************************************/
OP(dd,00) { op_00();									} /* DB   DD		  */
OP(dd,01) { op_01();									} /* DB   DD		  */
OP(dd,02) { op_02();									} /* DB   DD		  */
OP(dd,03) { op_03();									} /* DB   DD		  */
OP(dd,04) { op_04();									} /* DB   DD		  */
OP(dd,05) { op_05();									} /* DB   DD		  */
OP(dd,06) { op_06();									} /* DB   DD		  */
OP(dd,07) { op_07();									} /* DB   DD		  */

OP(dd,08) { op_08();									} /* DB   DD		  */
OP(dd,09) { _R++; ADD16(IX,BC); 									} /* ADD  IX,BC 	  */
OP(dd,0a) { op_0a();									} /* DB   DD		  */
OP(dd,0b) { op_0b();									} /* DB   DD		  */
OP(dd,0c) { op_0c();									} /* DB   DD		  */
OP(dd,0d) { op_0d();									} /* DB   DD		  */
OP(dd,0e) { op_0e();									} /* DB   DD		  */
OP(dd,0f) { op_0f();									} /* DB   DD		  */

OP(dd,10) { op_10();									} /* DB   DD		  */
OP(dd,11) { op_11();									} /* DB   DD		  */
OP(dd,12) { op_12();									} /* DB   DD		  */
OP(dd,13) { op_13();									} /* DB   DD		  */
OP(dd,14) { op_14();									} /* DB   DD		  */
OP(dd,15) { op_15();									} /* DB   DD		  */
OP(dd,16) { op_16();									} /* DB   DD		  */
OP(dd,17) { op_17();									} /* DB   DD		  */

OP(dd,18) { op_18();									} /* DB   DD		  */
OP(dd,19) { _R++; ADD16(IX,DE); 									} /* ADD  IX,DE 	  */
OP(dd,1a) { op_1a();									} /* DB   DD		  */
OP(dd,1b) { op_1b();									} /* DB   DD		  */
OP(dd,1c) { op_1c();									} /* DB   DD		  */
OP(dd,1d) { op_1d();									} /* DB   DD		  */
OP(dd,1e) { op_1e();									} /* DB   DD		  */
OP(dd,1f) { op_1f();									} /* DB   DD		  */

OP(dd,20) { op_20();									} /* DB   DD		  */
OP(dd,21) { _R++; _IX = ARG16();									} /* LD   IX,w		  */
OP(dd,22) { _R++; EA = ARG16(); WM16( EA, &Z80.IX );				} /* LD   (w),IX	  */
OP(dd,23) { _R++; _IX++;											} /* INC  IX		  */
OP(dd,24) { _R++; _HX = INC(_HX);									} /* INC  HX		  */
OP(dd,25) { _R++; _HX = DEC(_HX);									} /* DEC  HX		  */
OP(dd,26) { _R++; _HX = ARG();										} /* LD   HX,n		  */
OP(dd,27) { op_27();									} /* DB   DD		  */

OP(dd,28) { op_28();									} /* DB   DD		  */
OP(dd,29) { _R++; ADD16(IX,IX); 									} /* ADD  IX,IX 	  */
OP(dd,2a) { _R++; EA = ARG16(); RM16( EA, &Z80.IX );				} /* LD   IX,(w)	  */
OP(dd,2b) { _R++; _IX--;											} /* DEC  IX		  */
OP(dd,2c) { _R++; _LX = INC(_LX);									} /* INC  LX		  */
OP(dd,2d) { _R++; _LX = DEC(_LX);									} /* DEC  LX		  */
OP(dd,2e) { _R++; _LX = ARG();										} /* LD   LX,n		  */
OP(dd,2f) { op_2f();									} /* DB   DD		  */

OP(dd,30) { op_30();									} /* DB   DD		  */
OP(dd,31) { op_31();									} /* DB   DD		  */
OP(dd,32) { op_32();									} /* DB   DD		  */
OP(dd,33) { op_33();									} /* DB   DD		  */
OP(dd,34) { _R++; EAX; WM( EA, INC(RM(EA)) );						} /* INC  (IX+o)	  */
OP(dd,35) { _R++; EAX; WM( EA, DEC(RM(EA)) );						} /* DEC  (IX+o)	  */
OP(dd,36) { _R++; EAX; WM( EA, ARG() ); 							} /* LD   (IX+o),n	  */
OP(dd,37) { op_37();									} /* DB   DD		  */

OP(dd,38) { op_38();									} /* DB   DD		  */
OP(dd,39) { _R++; ADD16(IX,SP); 									} /* ADD  IX,SP 	  */
OP(dd,3a) { op_3a();									} /* DB   DD		  */
OP(dd,3b) { op_3b();									} /* DB   DD		  */
OP(dd,3c) { op_3c();									} /* DB   DD		  */
OP(dd,3d) { op_3d();									} /* DB   DD		  */
OP(dd,3e) { op_3e();									} /* DB   DD		  */
OP(dd,3f) { op_3f();									} /* DB   DD		  */

OP(dd,40) { op_40();									} /* DB   DD		  */
OP(dd,41) { op_41();									} /* DB   DD		  */
OP(dd,42) { op_42();									} /* DB   DD		  */
OP(dd,43) { op_43();									} /* DB   DD		  */
OP(dd,44) { _R++; _B = _HX; 										} /* LD   B,HX		  */
OP(dd,45) { _R++; _B = _LX; 										} /* LD   B,LX		  */
OP(dd,46) { _R++; EAX; _B = RM(EA); 								} /* LD   B,(IX+o)	  */
OP(dd,47) { op_47();									} /* DB   DD		  */

OP(dd,48) { op_48();									} /* DB   DD		  */
OP(dd,49) { op_49();									} /* DB   DD		  */
OP(dd,4a) { op_4a();									} /* DB   DD		  */
OP(dd,4b) { op_4b();									} /* DB   DD		  */
OP(dd,4c) { _R++; _C = _HX; 										} /* LD   C,HX		  */
OP(dd,4d) { _R++; _C = _LX; 										} /* LD   C,LX		  */
OP(dd,4e) { _R++; EAX; _C = RM(EA); 								} /* LD   C,(IX+o)	  */
OP(dd,4f) { op_4f();									} /* DB   DD		  */

OP(dd,50) { op_50();									} /* DB   DD		  */
OP(dd,51) { op_51();									} /* DB   DD		  */
OP(dd,52) { op_52();									} /* DB   DD		  */
OP(dd,53) { op_53();									} /* DB   DD		  */
OP(dd,54) { _R++; _D = _HX; 										} /* LD   D,HX		  */
OP(dd,55) { _R++; _D = _LX; 										} /* LD   D,LX		  */
OP(dd,56) { _R++; EAX; _D = RM(EA); 								} /* LD   D,(IX+o)	  */
OP(dd,57) { op_57();									} /* DB   DD		  */

OP(dd,58) { op_58();									} /* DB   DD		  */
OP(dd,59) { op_59();									} /* DB   DD		  */
OP(dd,5a) { op_5a();									} /* DB   DD		  */
OP(dd,5b) { op_5b();									} /* DB   DD		  */
OP(dd,5c) { _R++; _E = _HX; 										} /* LD   E,HX		  */
OP(dd,5d) { _R++; _E = _LX; 										} /* LD   E,LX		  */
OP(dd,5e) { _R++; EAX; _E = RM(EA); 								} /* LD   E,(IX+o)	  */
OP(dd,5f) { op_5f();									} /* DB   DD		  */

OP(dd,60) { _R++; _HX = _B; 										} /* LD   HX,B		  */
OP(dd,61) { _R++; _HX = _C; 										} /* LD   HX,C		  */
OP(dd,62) { _R++; _HX = _D; 										} /* LD   HX,D		  */
OP(dd,63) { _R++; _HX = _E; 										} /* LD   HX,E		  */
OP(dd,64) { 														} /* LD   HX,HX 	  */
OP(dd,65) { _R++; _HX = _LX;										} /* LD   HX,LX 	  */
OP(dd,66) { _R++; EAX; _H = RM(EA); 								} /* LD   H,(IX+o)	  */
OP(dd,67) { _R++; _HX = _A; 										} /* LD   HX,A		  */

OP(dd,68) { _R++; _LX = _B; 										} /* LD   LX,B		  */
OP(dd,69) { _R++; _LX = _C; 										} /* LD   LX,C		  */
OP(dd,6a) { _R++; _LX = _D; 										} /* LD   LX,D		  */
OP(dd,6b) { _R++; _LX = _E; 										} /* LD   LX,E		  */
OP(dd,6c) { _R++; _LX = _HX;										} /* LD   LX,HX 	  */
OP(dd,6d) { 														} /* LD   LX,LX 	  */
OP(dd,6e) { _R++; EAX; _L = RM(EA); 								} /* LD   L,(IX+o)	  */
OP(dd,6f) { _R++; _LX = _A; 										} /* LD   LX,A		  */

OP(dd,70) { _R++; EAX; WM( EA, _B );								} /* LD   (IX+o),B	  */
OP(dd,71) { _R++; EAX; WM( EA, _C );								} /* LD   (IX+o),C	  */
OP(dd,72) { _R++; EAX; WM( EA, _D );								} /* LD   (IX+o),D	  */
OP(dd,73) { _R++; EAX; WM( EA, _E );								} /* LD   (IX+o),E	  */
OP(dd,74) { _R++; EAX; WM( EA, _H );								} /* LD   (IX+o),H	  */
OP(dd,75) { _R++; EAX; WM( EA, _L );								} /* LD   (IX+o),L	  */
OP(dd,76) { op_76();									}		  /* DB   DD		  */
OP(dd,77) { _R++; EAX; WM( EA, _A );								} /* LD   (IX+o),A	  */

OP(dd,78) { op_78();									} /* DB   DD		  */
OP(dd,79) { op_79();									} /* DB   DD		  */
OP(dd,7a) { op_7a();									} /* DB   DD		  */
OP(dd,7b) { op_7b();									} /* DB   DD		  */
OP(dd,7c) { _R++; _A = _HX; 										} /* LD   A,HX		  */
OP(dd,7d) { _R++; _A = _LX; 										} /* LD   A,LX		  */
OP(dd,7e) { _R++; EAX; _A = RM(EA); 								} /* LD   A,(IX+o)	  */
OP(dd,7f) { op_7f();									} /* DB   DD		  */

OP(dd,80) { op_80();									} /* DB   DD		  */
OP(dd,81) { op_81();									} /* DB   DD		  */
OP(dd,82) { op_82();									} /* DB   DD		  */
OP(dd,83) { op_83();									} /* DB   DD		  */
OP(dd,84) { _R++; ADD(_HX); 										} /* ADD  A,HX		  */
OP(dd,85) { _R++; ADD(_LX); 										} /* ADD  A,LX		  */
OP(dd,86) { _R++; EAX; ADD(RM(EA)); 								} /* ADD  A,(IX+o)	  */
OP(dd,87) { op_87();									} /* DB   DD		  */

OP(dd,88) { op_88();									} /* DB   DD		  */
OP(dd,89) { op_89();									} /* DB   DD		  */
OP(dd,8a) { op_8a();									} /* DB   DD		  */
OP(dd,8b) { op_8b();									} /* DB   DD		  */
OP(dd,8c) { _R++; ADC(_HX); 										} /* ADC  A,HX		  */
OP(dd,8d) { _R++; ADC(_LX); 										} /* ADC  A,LX		  */
OP(dd,8e) { _R++; EAX; ADC(RM(EA)); 								} /* ADC  A,(IX+o)	  */
OP(dd,8f) { op_8f();									} /* DB   DD		  */

OP(dd,90) { op_90();									} /* DB   DD		  */
OP(dd,91) { op_91();									} /* DB   DD		  */
OP(dd,92) { op_92();									} /* DB   DD		  */
OP(dd,93) { op_93();									} /* DB   DD		  */
OP(dd,94) { _R++; SUB(_HX); 										} /* SUB  HX		  */
OP(dd,95) { _R++; SUB(_LX); 										} /* SUB  LX		  */
OP(dd,96) { _R++; EAX; SUB(RM(EA)); 								} /* SUB  (IX+o)	  */
OP(dd,97) { op_97();									} /* DB   DD		  */

OP(dd,98) { op_98();									} /* DB   DD		  */
OP(dd,99) { op_99();									} /* DB   DD		  */
OP(dd,9a) { op_9a();									} /* DB   DD		  */
OP(dd,9b) { op_9b();									} /* DB   DD		  */
OP(dd,9c) { _R++; SBC(_HX); 										} /* SBC  A,HX		  */
OP(dd,9d) { _R++; SBC(_LX); 										} /* SBC  A,LX		  */
OP(dd,9e) { _R++; EAX; SBC(RM(EA)); 								} /* SBC  A,(IX+o)	  */
OP(dd,9f) { op_9f();									} /* DB   DD		  */

OP(dd,a0) { op_a0();									} /* DB   DD		  */
OP(dd,a1) { op_a1();									} /* DB   DD		  */
OP(dd,a2) { op_a2();									} /* DB   DD		  */
OP(dd,a3) { op_a3();									} /* DB   DD		  */
OP(dd,a4) { _R++; AND(_HX); 										} /* AND  HX		  */
OP(dd,a5) { _R++; AND(_LX); 										} /* AND  LX		  */
OP(dd,a6) { _R++; EAX; AND(RM(EA)); 								} /* AND  (IX+o)	  */
OP(dd,a7) { op_a7();									} /* DB   DD		  */

OP(dd,a8) { op_a8();									} /* DB   DD		  */
OP(dd,a9) { op_a9();									} /* DB   DD		  */
OP(dd,aa) { op_aa();									} /* DB   DD		  */
OP(dd,ab) { op_ab();									} /* DB   DD		  */
OP(dd,ac) { _R++; XOR(_HX); 										} /* XOR  HX		  */
OP(dd,ad) { _R++; XOR(_LX); 										} /* XOR  LX		  */
OP(dd,ae) { _R++; EAX; XOR(RM(EA)); 								} /* XOR  (IX+o)	  */
OP(dd,af) { op_af();									} /* DB   DD		  */

OP(dd,b0) { op_b0();									} /* DB   DD		  */
OP(dd,b1) { op_b1();									} /* DB   DD		  */
OP(dd,b2) { op_b2();									} /* DB   DD		  */
OP(dd,b3) { op_b3();									} /* DB   DD		  */
OP(dd,b4) { _R++; OR(_HX);											} /* OR   HX		  */
OP(dd,b5) { _R++; OR(_LX);											} /* OR   LX		  */
OP(dd,b6) { _R++; EAX; OR(RM(EA));									} /* OR   (IX+o)	  */
OP(dd,b7) { op_b7();									} /* DB   DD		  */

OP(dd,b8) { op_b8();									} /* DB   DD		  */
OP(dd,b9) { op_b9();									} /* DB   DD		  */
OP(dd,ba) { op_ba();									} /* DB   DD		  */
OP(dd,bb) { op_bb();									} /* DB   DD		  */
OP(dd,bc) { _R++; CP(_HX);											} /* CP   HX		  */
OP(dd,bd) { _R++; CP(_LX);											} /* CP   LX		  */
OP(dd,be) { _R++; EAX; CP(RM(EA));									} /* CP   (IX+o)	  */
OP(dd,bf) { op_bf();									} /* DB   DD		  */

OP(dd,c0) { op_c0();									} /* DB   DD		  */
OP(dd,c1) { op_c1();									} /* DB   DD		  */
OP(dd,c2) { op_c2();									} /* DB   DD		  */
OP(dd,c3) { op_c3();									} /* DB   DD		  */
OP(dd,c4) { op_c4();									} /* DB   DD		  */
OP(dd,c5) { op_c5();									} /* DB   DD		  */
OP(dd,c6) { op_c6();									} /* DB   DD		  */
OP(dd,c7) { op_c7();									}		  /* DB   DD		  */

OP(dd,c8) { op_c8();									} /* DB   DD		  */
OP(dd,c9) { op_c9();									} /* DB   DD		  */
OP(dd,ca) { op_ca();									} /* DB   DD		  */
OP(dd,cb) { _R++; EAX; EXEC(xycb,ARG());							} /* **   DD CB xx	  */
OP(dd,cc) { op_cc();									} /* DB   DD		  */
OP(dd,cd) { op_cd();									} /* DB   DD		  */
OP(dd,ce) { op_ce();									} /* DB   DD		  */
OP(dd,cf) { op_cf();									} /* DB   DD		  */

OP(dd,d0) { op_d0();									} /* DB   DD		  */
OP(dd,d1) { op_d1();									} /* DB   DD		  */
OP(dd,d2) { op_d2();									} /* DB   DD		  */
OP(dd,d3) { op_d3();									} /* DB   DD		  */
OP(dd,d4) { op_d4();									} /* DB   DD		  */
OP(dd,d5) { op_d5();									} /* DB   DD		  */
OP(dd,d6) { op_d6();									} /* DB   DD		  */
OP(dd,d7) { op_d7();									} /* DB   DD		  */

OP(dd,d8) { op_d8();									} /* DB   DD		  */
OP(dd,d9) { op_d9();									} /* DB   DD		  */
OP(dd,da) { op_da();									} /* DB   DD		  */
OP(dd,db) { op_db();									} /* DB   DD		  */
OP(dd,dc) { op_dc();									} /* DB   DD		  */
OP(dd,dd) { op_dd();									} /* DB   DD		  */
OP(dd,de) { op_de();									} /* DB   DD		  */
OP(dd,df) { op_df();									} /* DB   DD		  */

OP(dd,e0) { op_e0();									} /* DB   DD		  */
OP(dd,e1) { _R++; POP(IX);											} /* POP  IX		  */
OP(dd,e2) { op_e2();									} /* DB   DD		  */
OP(dd,e3) { _R++; EXSP(IX); 										} /* EX   (SP),IX	  */
OP(dd,e4) { op_e4();									} /* DB   DD		  */
OP(dd,e5) { _R++; PUSH( IX );										} /* PUSH IX		  */
OP(dd,e6) { op_e6();									} /* DB   DD		  */
OP(dd,e7) { op_e7();									} /* DB   DD		  */

OP(dd,e8) { op_e8();									} /* DB   DD		  */
OP(dd,e9) { _R++; _PC = _IX; change_pc16(_PCD); 					} /* JP   (IX)		  */
OP(dd,ea) { op_ea();									} /* DB   DD		  */
OP(dd,eb) { op_eb();									} /* DB   DD		  */
OP(dd,ec) { op_ec();									} /* DB   DD		  */
OP(dd,ed) { op_ed();									} /* DB   DD		  */
OP(dd,ee) { op_ee();									} /* DB   DD		  */
OP(dd,ef) { op_ef();									} /* DB   DD		  */

OP(dd,f0) { op_f0();									} /* DB   DD		  */
OP(dd,f1) { op_f1();									} /* DB   DD		  */
OP(dd,f2) { op_f2();									} /* DB   DD		  */
OP(dd,f3) { op_f3();									} /* DB   DD		  */
OP(dd,f4) { op_f4();									} /* DB   DD		  */
OP(dd,f5) { op_f5();									} /* DB   DD		  */
OP(dd,f6) { op_f6();									} /* DB   DD		  */
OP(dd,f7) { op_f7();									} /* DB   DD		  */

OP(dd,f8) { op_f8();									} /* DB   DD		  */
OP(dd,f9) { _R++; _SP = _IX;										} /* LD   SP,IX 	  */
OP(dd,fa) { op_fa();									} /* DB   DD		  */
OP(dd,fb) { op_fb();									} /* DB   DD		  */
OP(dd,fc) { op_fc();									} /* DB   DD		  */
OP(dd,fd) { op_fd();									} /* DB   DD		  */
OP(dd,fe) { op_fe();									} /* DB   DD		  */
OP(dd,ff) { op_ff();									} /* DB   DD		  */

/**********************************************************
 * IY register related opcodes (FD prefix)
 **********************************************************/
OP(fd,00) { op_00();									} /* DB   FD		  */
OP(fd,01) { op_01();									} /* DB   FD		  */
OP(fd,02) { op_02();									} /* DB   FD		  */
OP(fd,03) { op_03();									} /* DB   FD		  */
OP(fd,04) { op_04();									} /* DB   FD		  */
OP(fd,05) { op_05();									} /* DB   FD		  */
OP(fd,06) { op_06();									} /* DB   FD		  */
OP(fd,07) { op_07();									} /* DB   FD		  */

OP(fd,08) { op_08();									} /* DB   FD		  */
OP(fd,09) { _R++; ADD16(IY,BC); 									} /* ADD  IY,BC 	  */
OP(fd,0a) { op_0a();									} /* DB   FD		  */
OP(fd,0b) { op_0b();									} /* DB   FD		  */
OP(fd,0c) { op_0c();									} /* DB   FD		  */
OP(fd,0d) { op_0d();									} /* DB   FD		  */
OP(fd,0e) { op_0e();									} /* DB   FD		  */
OP(fd,0f) { op_0f();									} /* DB   FD		  */

OP(fd,10) { op_10();									} /* DB   FD		  */
OP(fd,11) { op_11();									} /* DB   FD		  */
OP(fd,12) { op_12();									} /* DB   FD		  */
OP(fd,13) { op_13();									} /* DB   FD		  */
OP(fd,14) { op_14();									} /* DB   FD		  */
OP(fd,15) { op_15();									} /* DB   FD		  */
OP(fd,16) { op_16();									} /* DB   FD		  */
OP(fd,17) { op_17();									} /* DB   FD		  */

OP(fd,18) { op_18();									} /* DB   FD		  */
OP(fd,19) { _R++; ADD16(IY,DE); 									} /* ADD  IY,DE 	  */
OP(fd,1a) { op_1a();									} /* DB   FD		  */
OP(fd,1b) { op_1b();									} /* DB   FD		  */
OP(fd,1c) { op_1c();									} /* DB   FD		  */
OP(fd,1d) { op_1d();									} /* DB   FD		  */
OP(fd,1e) { op_1e();									} /* DB   FD		  */
OP(fd,1f) { op_1f();									} /* DB   FD		  */

OP(fd,20) { op_20();									} /* DB   FD		  */
OP(fd,21) { _R++; _IY = ARG16();									} /* LD   IY,w		  */
OP(fd,22) { _R++; EA = ARG16(); WM16( EA, &Z80.IY );				} /* LD   (w),IY	  */
OP(fd,23) { _R++; _IY++;											} /* INC  IY		  */
OP(fd,24) { _R++; _HY = INC(_HY);									} /* INC  HY		  */
OP(fd,25) { _R++; _HY = DEC(_HY);									} /* DEC  HY		  */
OP(fd,26) { _R++; _HY = ARG();										} /* LD   HY,n		  */
OP(fd,27) { op_27();									} /* DB   FD		  */

OP(fd,28) { op_28();									} /* DB   FD		  */
OP(fd,29) { _R++; ADD16(IY,IY); 									} /* ADD  IY,IY 	  */
OP(fd,2a) { _R++; EA = ARG16(); RM16( EA, &Z80.IY );				} /* LD   IY,(w)	  */
OP(fd,2b) { _R++; _IY--;											} /* DEC  IY		  */
OP(fd,2c) { _R++; _LY = INC(_LY);									} /* INC  LY		  */
OP(fd,2d) { _R++; _LY = DEC(_LY);									} /* DEC  LY		  */
OP(fd,2e) { _R++; _LY = ARG();										} /* LD   LY,n		  */
OP(fd,2f) { op_2f();									} /* DB   FD		  */

OP(fd,30) { op_30();									} /* DB   FD		  */
OP(fd,31) { op_31();									} /* DB   FD		  */
OP(fd,32) { op_32();									} /* DB   FD		  */
OP(fd,33) { op_33();									} /* DB   FD		  */
OP(fd,34) { _R++; EAY; WM( EA, INC(RM(EA)) );						} /* INC  (IY+o)	  */
OP(fd,35) { _R++; EAY; WM( EA, DEC(RM(EA)) );						} /* DEC  (IY+o)	  */
OP(fd,36) { _R++; EAY; WM( EA, ARG() ); 							} /* LD   (IY+o),n	  */
OP(fd,37) { op_37();									} /* DB   FD		  */

OP(fd,38) { op_38();									} /* DB   FD		  */
OP(fd,39) { _R++; ADD16(IY,SP); 									} /* ADD  IY,SP 	  */
OP(fd,3a) { op_3a();									} /* DB   FD		  */
OP(fd,3b) { op_3b();									} /* DB   FD		  */
OP(fd,3c) { op_3c();									} /* DB   FD		  */
OP(fd,3d) { op_3d();									} /* DB   FD		  */
OP(fd,3e) { op_3e();									} /* DB   FD		  */
OP(fd,3f) { op_3f();									} /* DB   FD		  */

OP(fd,40) { op_40();									} /* DB   FD		  */
OP(fd,41) { op_41();									} /* DB   FD		  */
OP(fd,42) { op_42();									} /* DB   FD		  */
OP(fd,43) { op_43();									} /* DB   FD		  */
OP(fd,44) { _R++; _B = _HY; 										} /* LD   B,HY		  */
OP(fd,45) { _R++; _B = _LY; 										} /* LD   B,LY		  */
OP(fd,46) { _R++; EAY; _B = RM(EA); 								} /* LD   B,(IY+o)	  */
OP(fd,47) { op_47();									} /* DB   FD		  */

OP(fd,48) { op_48();									} /* DB   FD		  */
OP(fd,49) { op_49();									} /* DB   FD		  */
OP(fd,4a) { op_4a();									} /* DB   FD		  */
OP(fd,4b) { op_4b();									} /* DB   FD		  */
OP(fd,4c) { _R++; _C = _HY; 										} /* LD   C,HY		  */
OP(fd,4d) { _R++; _C = _LY; 										} /* LD   C,LY		  */
OP(fd,4e) { _R++; EAY; _C = RM(EA); 								} /* LD   C,(IY+o)	  */
OP(fd,4f) { op_4f();									} /* DB   FD		  */

OP(fd,50) { op_50();									} /* DB   FD		  */
OP(fd,51) { op_51();									} /* DB   FD		  */
OP(fd,52) { op_52();									} /* DB   FD		  */
OP(fd,53) { op_53();									} /* DB   FD		  */
OP(fd,54) { _R++; _D = _HY; 										} /* LD   D,HY		  */
OP(fd,55) { _R++; _D = _LY; 										} /* LD   D,LY		  */
OP(fd,56) { _R++; EAY; _D = RM(EA); 								} /* LD   D,(IY+o)	  */
OP(fd,57) { op_57();									} /* DB   FD		  */

OP(fd,58) { op_58();									} /* DB   FD		  */
OP(fd,59) { op_59();									} /* DB   FD		  */
OP(fd,5a) { op_5a();									} /* DB   FD		  */
OP(fd,5b) { op_5b();									} /* DB   FD		  */
OP(fd,5c) { _R++; _E = _HY; 										} /* LD   E,HY		  */
OP(fd,5d) { _R++; _E = _LY; 										} /* LD   E,LY		  */
OP(fd,5e) { _R++; EAY; _E = RM(EA); 								} /* LD   E,(IY+o)	  */
OP(fd,5f) { op_5f();									} /* DB   FD		  */

OP(fd,60) { _R++; _HY = _B; 										} /* LD   HY,B		  */
OP(fd,61) { _R++; _HY = _C; 										} /* LD   HY,C		  */
OP(fd,62) { _R++; _HY = _D; 										} /* LD   HY,D		  */
OP(fd,63) { _R++; _HY = _E; 										} /* LD   HY,E		  */
OP(fd,64) { _R++;													} /* LD   HY,HY 	  */
OP(fd,65) { _R++; _HY = _LY;										} /* LD   HY,LY 	  */
OP(fd,66) { _R++; EAY; _H = RM(EA); 								} /* LD   H,(IY+o)	  */
OP(fd,67) { _R++; _HY = _A; 										} /* LD   HY,A		  */

OP(fd,68) { _R++; _LY = _B; 										} /* LD   LY,B		  */
OP(fd,69) { _R++; _LY = _C; 										} /* LD   LY,C		  */
OP(fd,6a) { _R++; _LY = _D; 										} /* LD   LY,D		  */
OP(fd,6b) { _R++; _LY = _E; 										} /* LD   LY,E		  */
OP(fd,6c) { _R++; _LY = _HY;										} /* LD   LY,HY 	  */
OP(fd,6d) { _R++;													} /* LD   LY,LY 	  */
OP(fd,6e) { _R++; EAY; _L = RM(EA); 								} /* LD   L,(IY+o)	  */
OP(fd,6f) { _R++; _LY = _A; 										} /* LD   LY,A		  */

OP(fd,70) { _R++; EAY; WM( EA, _B );								} /* LD   (IY+o),B	  */
OP(fd,71) { _R++; EAY; WM( EA, _C );								} /* LD   (IY+o),C	  */
OP(fd,72) { _R++; EAY; WM( EA, _D );								} /* LD   (IY+o),D	  */
OP(fd,73) { _R++; EAY; WM( EA, _E );								} /* LD   (IY+o),E	  */
OP(fd,74) { _R++; EAY; WM( EA, _H );								} /* LD   (IY+o),H	  */
OP(fd,75) { _R++; EAY; WM( EA, _L );								} /* LD   (IY+o),L	  */
OP(fd,76) { op_76();									}		  /* DB   FD		  */
OP(fd,77) { _R++; EAY; WM( EA, _A );								} /* LD   (IY+o),A	  */

OP(fd,78) { op_78();									} /* DB   FD		  */
OP(fd,79) { op_79();									} /* DB   FD		  */
OP(fd,7a) { op_7a();									} /* DB   FD		  */
OP(fd,7b) { op_7b();									} /* DB   FD		  */
OP(fd,7c) { _R++; _A = _HY; 										} /* LD   A,HY		  */
OP(fd,7d) { _R++; _A = _LY; 										} /* LD   A,LY		  */
OP(fd,7e) { _R++; EAY; _A = RM(EA); 								} /* LD   A,(IY+o)	  */
OP(fd,7f) { op_7f();									} /* DB   FD		  */

OP(fd,80) { op_80();									} /* DB   FD		  */
OP(fd,81) { op_81();									} /* DB   FD		  */
OP(fd,82) { op_82();									} /* DB   FD		  */
OP(fd,83) { op_83();									} /* DB   FD		  */
OP(fd,84) { _R++; ADD(_HY); 										} /* ADD  A,HY		  */
OP(fd,85) { _R++; ADD(_LY); 										} /* ADD  A,LY		  */
OP(fd,86) { _R++; EAY; ADD(RM(EA)); 								} /* ADD  A,(IY+o)	  */
OP(fd,87) { op_87();									} /* DB   FD		  */

OP(fd,88) { op_88();									} /* DB   FD		  */
OP(fd,89) { op_89();									} /* DB   FD		  */
OP(fd,8a) { op_8a();									} /* DB   FD		  */
OP(fd,8b) { op_8b();									} /* DB   FD		  */
OP(fd,8c) { _R++; ADC(_HY); 										} /* ADC  A,HY		  */
OP(fd,8d) { _R++; ADC(_LY); 										} /* ADC  A,LY		  */
OP(fd,8e) { _R++; EAY; ADC(RM(EA)); 								} /* ADC  A,(IY+o)	  */
OP(fd,8f) { op_8f();									} /* DB   FD		  */

OP(fd,90) { op_90();									} /* DB   FD		  */
OP(fd,91) { op_91();									} /* DB   FD		  */
OP(fd,92) { op_92();									} /* DB   FD		  */
OP(fd,93) { op_93();									} /* DB   FD		  */
OP(fd,94) { _R++; SUB(_HY); 										} /* SUB  HY		  */
OP(fd,95) { _R++; SUB(_LY); 										} /* SUB  LY		  */
OP(fd,96) { _R++; EAY; SUB(RM(EA)); 								} /* SUB  (IY+o)	  */
OP(fd,97) { op_97();									} /* DB   FD		  */

OP(fd,98) { op_98();									} /* DB   FD		  */
OP(fd,99) { op_99();									} /* DB   FD		  */
OP(fd,9a) { op_9a();									} /* DB   FD		  */
OP(fd,9b) { op_9b();									} /* DB   FD		  */
OP(fd,9c) { _R++; SBC(_HY); 										} /* SBC  A,HY		  */
OP(fd,9d) { _R++; SBC(_LY); 										} /* SBC  A,LY		  */
OP(fd,9e) { _R++; EAY; SBC(RM(EA)); 								} /* SBC  A,(IY+o)	  */
OP(fd,9f) { op_9f();									} /* DB   FD		  */

OP(fd,a0) { op_a0();									} /* DB   FD		  */
OP(fd,a1) { op_a1();									} /* DB   FD		  */
OP(fd,a2) { op_a2();									} /* DB   FD		  */
OP(fd,a3) { op_a3();									} /* DB   FD		  */
OP(fd,a4) { _R++; AND(_HY); 										} /* AND  HY		  */
OP(fd,a5) { _R++; AND(_LY); 										} /* AND  LY		  */
OP(fd,a6) { _R++; EAY; AND(RM(EA)); 								} /* AND  (IY+o)	  */
OP(fd,a7) { op_a7();									} /* DB   FD		  */

OP(fd,a8) { op_a8();									} /* DB   FD		  */
OP(fd,a9) { op_a9();									} /* DB   FD		  */
OP(fd,aa) { op_aa();									} /* DB   FD		  */
OP(fd,ab) { op_ab();									} /* DB   FD		  */
OP(fd,ac) { _R++; XOR(_HY); 										} /* XOR  HY		  */
OP(fd,ad) { _R++; XOR(_LY); 										} /* XOR  LY		  */
OP(fd,ae) { _R++; EAY; XOR(RM(EA)); 								} /* XOR  (IY+o)	  */
OP(fd,af) { op_af();									} /* DB   FD		  */

OP(fd,b0) { op_b0();									} /* DB   FD		  */
OP(fd,b1) { op_b1();									} /* DB   FD		  */
OP(fd,b2) { op_b2();									} /* DB   FD		  */
OP(fd,b3) { op_b3();									} /* DB   FD		  */
OP(fd,b4) { _R++; OR(_HY);											} /* OR   HY		  */
OP(fd,b5) { _R++; OR(_LY);											} /* OR   LY		  */
OP(fd,b6) { _R++; EAY; OR(RM(EA));									} /* OR   (IY+o)	  */
OP(fd,b7) { op_b7();									} /* DB   FD		  */

OP(fd,b8) { op_b8();									} /* DB   FD		  */
OP(fd,b9) { op_b9();									} /* DB   FD		  */
OP(fd,ba) { op_ba();									} /* DB   FD		  */
OP(fd,bb) { op_bb();									} /* DB   FD		  */
OP(fd,bc) { _R++; CP(_HY);											} /* CP   HY		  */
OP(fd,bd) { _R++; CP(_LY);											} /* CP   LY		  */
OP(fd,be) { _R++; EAY; CP(RM(EA));									} /* CP   (IY+o)	  */
OP(fd,bf) { op_bf();									} /* DB   FD		  */

OP(fd,c0) { op_c0();									} /* DB   FD		  */
OP(fd,c1) { op_c1();									} /* DB   FD		  */
OP(fd,c2) { op_c2();									} /* DB   FD		  */
OP(fd,c3) { op_c3();									} /* DB   FD		  */
OP(fd,c4) { op_c4();									} /* DB   FD		  */
OP(fd,c5) { op_c5();									} /* DB   FD		  */
OP(fd,c6) { op_c6();									} /* DB   FD		  */
OP(fd,c7) { op_c7();									} /* DB   FD		  */

OP(fd,c8) { op_c8();									} /* DB   FD		  */
OP(fd,c9) { op_c9();									} /* DB   FD		  */
OP(fd,ca) { op_ca();									} /* DB   FD		  */
OP(fd,cb) { _R++; EAY; EXEC(xycb,ARG());							} /* **   FD CB xx	  */
OP(fd,cc) { op_cc();									} /* DB   FD		  */
OP(fd,cd) { op_cd();									} /* DB   FD		  */
OP(fd,ce) { op_ce();									} /* DB   FD		  */
OP(fd,cf) { op_cf();									} /* DB   FD		  */

OP(fd,d0) { op_d0();									} /* DB   FD		  */
OP(fd,d1) { op_d1();									} /* DB   FD		  */
OP(fd,d2) { op_d2();									} /* DB   FD		  */
OP(fd,d3) { op_d3();									} /* DB   FD		  */
OP(fd,d4) { op_d4();									} /* DB   FD		  */
OP(fd,d5) { op_d5();									} /* DB   FD		  */
OP(fd,d6) { op_d6();									} /* DB   FD		  */
OP(fd,d7) { op_d7();									} /* DB   FD		  */

OP(fd,d8) { op_d8();									} /* DB   FD		  */
OP(fd,d9) { op_d9();									} /* DB   FD		  */
OP(fd,da) { op_da();									} /* DB   FD		  */
OP(fd,db) { op_db();									} /* DB   FD		  */
OP(fd,dc) { op_dc();									} /* DB   FD		  */
OP(fd,dd) { op_dd();									} /* DB   FD		  */
OP(fd,de) { op_de();									} /* DB   FD		  */
OP(fd,df) { op_df();									} /* DB   FD		  */

OP(fd,e0) { op_e0();									} /* DB   FD		  */
OP(fd,e1) { _R++; POP(IY);											} /* POP  IY		  */
OP(fd,e2) { op_e2();									} /* DB   FD		  */
OP(fd,e3) { _R++; EXSP(IY); 										} /* EX   (SP),IY	  */
OP(fd,e4) { op_e4();									} /* DB   FD		  */
OP(fd,e5) { _R++; PUSH( IY );										} /* PUSH IY		  */
OP(fd,e6) { op_e6();									} /* DB   FD		  */
OP(fd,e7) { op_e7();									} /* DB   FD		  */

OP(fd,e8) { op_e8();									} /* DB   FD		  */
OP(fd,e9) { _R++; _PC = _IY; change_pc16(_PCD); 					} /* JP   (IY)		  */
OP(fd,ea) { op_ea();									} /* DB   FD		  */
OP(fd,eb) { op_eb();									} /* DB   FD		  */
OP(fd,ec) { op_ec();									} /* DB   FD		  */
OP(fd,ed) { op_ed();									} /* DB   FD		  */
OP(fd,ee) { op_ee();									} /* DB   FD		  */
OP(fd,ef) { op_ef();									} /* DB   FD		  */

OP(fd,f0) { op_f0();									} /* DB   FD		  */
OP(fd,f1) { op_f1();									} /* DB   FD		  */
OP(fd,f2) { op_f2();									} /* DB   FD		  */
OP(fd,f3) { op_f3();									} /* DB   FD		  */
OP(fd,f4) { op_f4();									} /* DB   FD		  */
OP(fd,f5) { op_f5();									} /* DB   FD		  */
OP(fd,f6) { op_f6();									} /* DB   FD		  */
OP(fd,f7) { op_f7();									} /* DB   FD		  */

OP(fd,f8) { op_f8();									} /* DB   FD		  */
OP(fd,f9) { _R++; _SP = _IY;										} /* LD   SP,IY 	  */
OP(fd,fa) { op_fa();									} /* DB   FD		  */
OP(fd,fb) { op_fb();									} /* DB   FD		  */
OP(fd,fc) { op_fc();									} /* DB   FD		  */
OP(fd,fd) { op_fd();									} /* DB   FD		  */
OP(fd,fe) { op_fe();									} /* DB   FD		  */
OP(fd,ff) { op_ff();									} /* DB   FD		  */

/**********************************************************
 * special opcodes (ED prefix)
 **********************************************************/
OP(ed,00) { 											} /* DB   ED		  */
OP(ed,01) { 											} /* DB   ED		  */
OP(ed,02) { 											} /* DB   ED		  */
OP(ed,03) { 											} /* DB   ED		  */
OP(ed,04) { 											} /* DB   ED		  */
OP(ed,05) { 											} /* DB   ED		  */
OP(ed,06) { 											} /* DB   ED		  */
OP(ed,07) { 											} /* DB   ED		  */

OP(ed,08) { 											} /* DB   ED		  */
OP(ed,09) { 											} /* DB   ED		  */
OP(ed,0a) { 											} /* DB   ED		  */
OP(ed,0b) { 											} /* DB   ED		  */
OP(ed,0c) { 											} /* DB   ED		  */
OP(ed,0d) { 											} /* DB   ED		  */
OP(ed,0e) { 											} /* DB   ED		  */
OP(ed,0f) { 											} /* DB   ED		  */

OP(ed,10) { 											} /* DB   ED		  */
OP(ed,11) { 											} /* DB   ED		  */
OP(ed,12) { 											} /* DB   ED		  */
OP(ed,13) { 											} /* DB   ED		  */
OP(ed,14) { 											} /* DB   ED		  */
OP(ed,15) { 											} /* DB   ED		  */
OP(ed,16) { 											} /* DB   ED		  */
OP(ed,17) { 											} /* DB   ED		  */

OP(ed,18) { 											} /* DB   ED		  */
OP(ed,19) { 											} /* DB   ED		  */
OP(ed,1a) { 											} /* DB   ED		  */
OP(ed,1b) { 											} /* DB   ED		  */
OP(ed,1c) { 											} /* DB   ED		  */
OP(ed,1d) { 											} /* DB   ED		  */
OP(ed,1e) { 											} /* DB   ED		  */
OP(ed,1f) { 											} /* DB   ED		  */

OP(ed,20) { 											} /* DB   ED		  */
OP(ed,21) { 											} /* DB   ED		  */
OP(ed,22) { 											} /* DB   ED		  */
OP(ed,23) { 											} /* DB   ED		  */
OP(ed,24) { 											} /* DB   ED		  */
OP(ed,25) { 											} /* DB   ED		  */
OP(ed,26) { 											} /* DB   ED		  */
OP(ed,27) { 											} /* DB   ED		  */

OP(ed,28) { 											} /* DB   ED		  */
OP(ed,29) { 											} /* DB   ED		  */
OP(ed,2a) { 											} /* DB   ED		  */
OP(ed,2b) { 											} /* DB   ED		  */
OP(ed,2c) { 											} /* DB   ED		  */
OP(ed,2d) { 											} /* DB   ED		  */
OP(ed,2e) { 											} /* DB   ED		  */
OP(ed,2f) { 											} /* DB   ED		  */

OP(ed,30) { 											} /* DB   ED		  */
OP(ed,31) { 											} /* DB   ED		  */
OP(ed,32) { 											} /* DB   ED		  */
OP(ed,33) { 											} /* DB   ED		  */
OP(ed,34) { 											} /* DB   ED		  */
OP(ed,35) { 											} /* DB   ED		  */
OP(ed,36) { 											} /* DB   ED		  */
OP(ed,37) { 											} /* DB   ED		  */

OP(ed,38) { 											} /* DB   ED		  */
OP(ed,39) { 											} /* DB   ED		  */
OP(ed,3a) { 											} /* DB   ED		  */
OP(ed,3b) { 											} /* DB   ED		  */
OP(ed,3c) { 											} /* DB   ED		  */
OP(ed,3d) { 											} /* DB   ED		  */
OP(ed,3e) { 											} /* DB   ED		  */
OP(ed,3f) { 											} /* DB   ED		  */

OP(ed,40) { _B = IN(_BC); _F = (_F & CF) | SZP[_B]; 				} /* IN   B,(C) 	  */
OP(ed,41) { OUT(_BC,_B);											} /* OUT  (C),B 	  */
OP(ed,42) { SBC16( BC );											} /* SBC  HL,BC 	  */
OP(ed,43) { EA = ARG16(); WM16( EA, &Z80.BC );						} /* LD   (w),BC	  */
OP(ed,44) { NEG;													} /* NEG			  */
OP(ed,45) { RETN;													} /* RETN;			  */
OP(ed,46) { _IM = 0;												} /* IM   0 		  */
OP(ed,47) { LD_I_A; 												} /* LD   I,A		  */

OP(ed,48) { _C = IN(_BC); _F = (_F & CF) | SZP[_C]; 				} /* IN   C,(C) 	  */
OP(ed,49) { OUT(_BC,_C);											} /* OUT  (C),C 	  */
OP(ed,4a) { ADC16( BC );											} /* ADC  HL,BC 	  */
OP(ed,4b) { EA = ARG16(); RM16( EA, &Z80.BC );						} /* LD   BC,(w)	  */
OP(ed,4c) { NEG;													} /* NEG			  */
OP(ed,4d) { RETI;													} /* RETI			  */
OP(ed,4e) { _IM = 0;												} /* IM   0 		  */
OP(ed,4f) { LD_R_A; 												} /* LD   R,A		  */

OP(ed,50) { _D = IN(_BC); _F = (_F & CF) | SZP[_D]; 				} /* IN   D,(C) 	  */
OP(ed,51) { OUT(_BC,_D);											} /* OUT  (C),D 	  */
OP(ed,52) { SBC16( DE );											} /* SBC  HL,DE 	  */
OP(ed,53) { EA = ARG16(); WM16( EA, &Z80.DE );						} /* LD   (w),DE	  */
OP(ed,54) { NEG;													} /* NEG			  */
OP(ed,55) { RETN;													} /* RETN;			  */
OP(ed,56) { _IM = 1;												} /* IM   1 		  */
OP(ed,57) { LD_A_I; 												} /* LD   A,I		  */

OP(ed,58) { _E = IN(_BC); _F = (_F & CF) | SZP[_E]; 				} /* IN   E,(C) 	  */
OP(ed,59) { OUT(_BC,_E);											} /* OUT  (C),E 	  */
OP(ed,5a) { ADC16( DE );											} /* ADC  HL,DE 	  */
OP(ed,5b) { EA = ARG16(); RM16( EA, &Z80.DE );						} /* LD   DE,(w)	  */
OP(ed,5c) { NEG;													} /* NEG			  */
OP(ed,5d) { RETI;													} /* RETI			  */
OP(ed,5e) { _IM = 2;												} /* IM   2 		  */
OP(ed,5f) { LD_A_R; 												} /* LD   A,R		  */

OP(ed,60) { _H = IN(_BC); _F = (_F & CF) | SZP[_H]; 				} /* IN   H,(C) 	  */
OP(ed,61) { OUT(_BC,_H);											} /* OUT  (C),H 	  */
OP(ed,62) { SBC16( HL );											} /* SBC  HL,HL 	  */
OP(ed,63) { EA = ARG16(); WM16( EA, &Z80.HL );						} /* LD   (w),HL	  */
OP(ed,64) { NEG;													} /* NEG			  */
OP(ed,65) { RETN;													} /* RETN;			  */
OP(ed,66) { _IM = 0;												} /* IM   0 		  */
OP(ed,67) { RRD;													} /* RRD  (HL)		  */

OP(ed,68) { _L = IN(_BC); _F = (_F & CF) | SZP[_L]; 				} /* IN   L,(C) 	  */
OP(ed,69) { OUT(_BC,_L);											} /* OUT  (C),L 	  */
OP(ed,6a) { ADC16( HL );											} /* ADC  HL,HL 	  */
OP(ed,6b) { EA = ARG16(); RM16( EA, &Z80.HL );						} /* LD   HL,(w)	  */
OP(ed,6c) { NEG;													} /* NEG			  */
OP(ed,6d) { RETI;													} /* RETI			  */
OP(ed,6e) { _IM = 0;												} /* IM   0 		  */
OP(ed,6f) { RLD;													} /* RLD  (HL)		  */

OP(ed,70) { UINT8 res = IN(_BC); _F = (_F & CF) | SZP[res]; 		} /* IN   0,(C) 	  */
OP(ed,71) { OUT(_BC,0); 											} /* OUT  (C),0 	  */
OP(ed,72) { SBC16( SP );											} /* SBC  HL,SP 	  */
OP(ed,73) { EA = ARG16(); WM16( EA, &Z80.SP );						} /* LD   (w),SP	  */
OP(ed,74) { NEG;													} /* NEG			  */
OP(ed,75) { RETN;													} /* RETN;			  */
OP(ed,76) { _IM = 1;												} /* IM   1 		  */
OP(ed,77) { 											} /* DB   ED,77 	  */

OP(ed,78) { _A = IN(_BC); _F = (_F & CF) | SZP[_A]; 				} /* IN   E,(C) 	  */
OP(ed,79) { OUT(_BC,_A);											} /* OUT  (C),E 	  */
OP(ed,7a) { ADC16( SP );											} /* ADC  HL,SP 	  */
OP(ed,7b) { EA = ARG16(); RM16( EA, &Z80.SP );						} /* LD   SP,(w)	  */
OP(ed,7c) { NEG;													} /* NEG			  */
OP(ed,7d) { RETI;													} /* RETI			  */
OP(ed,7e) { _IM = 2;												} /* IM   2 		  */
OP(ed,7f) { 											} /* DB   ED,7F 	  */

OP(ed,80) { 											} /* DB   ED		  */
OP(ed,81) { 											} /* DB   ED		  */
OP(ed,82) { 											} /* DB   ED		  */
OP(ed,83) { 											} /* DB   ED		  */
OP(ed,84) { 											} /* DB   ED		  */
OP(ed,85) { 											} /* DB   ED		  */
OP(ed,86) { 											} /* DB   ED		  */
OP(ed,87) { 											} /* DB   ED		  */

OP(ed,88) { 											} /* DB   ED		  */
OP(ed,89) { 											} /* DB   ED		  */
OP(ed,8a) { 											} /* DB   ED		  */
OP(ed,8b) { 											} /* DB   ED		  */
OP(ed,8c) { 											} /* DB   ED		  */
OP(ed,8d) { 											} /* DB   ED		  */
OP(ed,8e) { 											} /* DB   ED		  */
OP(ed,8f) { 											} /* DB   ED		  */

OP(ed,90) { 											} /* DB   ED		  */
OP(ed,91) { 											} /* DB   ED		  */
OP(ed,92) { 											} /* DB   ED		  */
OP(ed,93) { 											} /* DB   ED		  */
OP(ed,94) { 											} /* DB   ED		  */
OP(ed,95) { 											} /* DB   ED		  */
OP(ed,96) { 											} /* DB   ED		  */
OP(ed,97) { 											} /* DB   ED		  */

OP(ed,98) { 											} /* DB   ED		  */
OP(ed,99) { 											} /* DB   ED		  */
OP(ed,9a) { 											} /* DB   ED		  */
OP(ed,9b) { 											} /* DB   ED		  */
OP(ed,9c) { 											} /* DB   ED		  */
OP(ed,9d) { 											} /* DB   ED		  */
OP(ed,9e) { 											} /* DB   ED		  */
OP(ed,9f) { 											} /* DB   ED		  */

OP(ed,a0) { LDI;													} /* LDI			  */
OP(ed,a1) { CPI;													} /* CPI			  */
OP(ed,a2) { INI;													} /* INI			  */
OP(ed,a3) { OUTI;													} /* OUTI			  */
OP(ed,a4) { 											} /* DB   ED		  */
OP(ed,a5) { 											} /* DB   ED		  */
OP(ed,a6) { 											} /* DB   ED		  */
OP(ed,a7) { 											} /* DB   ED		  */

OP(ed,a8) { LDD;													} /* LDD			  */
OP(ed,a9) { CPD;													} /* CPD			  */
OP(ed,aa) { IND;													} /* IND			  */
OP(ed,ab) { OUTD;													} /* OUTD			  */
OP(ed,ac) { 											} /* DB   ED		  */
OP(ed,ad) { 											} /* DB   ED		  */
OP(ed,ae) { 											} /* DB   ED		  */
OP(ed,af) { 											} /* DB   ED		  */

OP(ed,b0) { LDIR;													} /* LDIR			  */
OP(ed,b1) { CPIR;													} /* CPIR			  */
OP(ed,b2) { INIR;													} /* INIR			  */
OP(ed,b3) { OTIR;													} /* OTIR			  */
OP(ed,b4) { 											} /* DB   ED		  */
OP(ed,b5) { 											} /* DB   ED		  */
OP(ed,b6) { 											} /* DB   ED		  */
OP(ed,b7) { 											} /* DB   ED		  */

OP(ed,b8) { LDDR;													} /* LDDR			  */
OP(ed,b9) { CPDR;													} /* CPDR			  */
OP(ed,ba) { INDR;													} /* INDR			  */
OP(ed,bb) { OTDR;													} /* OTDR			  */
OP(ed,bc) { 											} /* DB   ED		  */
OP(ed,bd) { 											} /* DB   ED		  */
OP(ed,be) { 											} /* DB   ED		  */
OP(ed,bf) { 											} /* DB   ED		  */

OP(ed,c0) { 											} /* DB   ED		  */
OP(ed,c1) { 											} /* DB   ED		  */
OP(ed,c2) { 											} /* DB   ED		  */
OP(ed,c3) { 											} /* DB   ED		  */
OP(ed,c4) { 											} /* DB   ED		  */
OP(ed,c5) { 											} /* DB   ED		  */
OP(ed,c6) { 											} /* DB   ED		  */
OP(ed,c7) { 											} /* DB   ED		  */

OP(ed,c8) { 											} /* DB   ED		  */
OP(ed,c9) { 											} /* DB   ED		  */
OP(ed,ca) { 											} /* DB   ED		  */
OP(ed,cb) { 											} /* DB   ED		  */
OP(ed,cc) { 											} /* DB   ED		  */
OP(ed,cd) { 											} /* DB   ED		  */
OP(ed,ce) { 											} /* DB   ED		  */
OP(ed,cf) { 											} /* DB   ED		  */

OP(ed,d0) { 											} /* DB   ED		  */
OP(ed,d1) { 											} /* DB   ED		  */
OP(ed,d2) { 											} /* DB   ED		  */
OP(ed,d3) { 											} /* DB   ED		  */
OP(ed,d4) { 											} /* DB   ED		  */
OP(ed,d5) { 											} /* DB   ED		  */
OP(ed,d6) { 											} /* DB   ED		  */
OP(ed,d7) { 											} /* DB   ED		  */

OP(ed,d8) { 											} /* DB   ED		  */
OP(ed,d9) { 											} /* DB   ED		  */
OP(ed,da) { 											} /* DB   ED		  */
OP(ed,db) { 											} /* DB   ED		  */
OP(ed,dc) { 											} /* DB   ED		  */
OP(ed,dd) { 											} /* DB   ED		  */
OP(ed,de) { 											} /* DB   ED		  */
OP(ed,df) { 											} /* DB   ED		  */

OP(ed,e0) { 											} /* DB   ED		  */
OP(ed,e1) { 											} /* DB   ED		  */
OP(ed,e2) { 											} /* DB   ED		  */
OP(ed,e3) { 											} /* DB   ED		  */
OP(ed,e4) { 											} /* DB   ED		  */
OP(ed,e5) { 											} /* DB   ED		  */
OP(ed,e6) { 											} /* DB   ED		  */
OP(ed,e7) { 											} /* DB   ED		  */

OP(ed,e8) { 											} /* DB   ED		  */
OP(ed,e9) { 											} /* DB   ED		  */
OP(ed,ea) { 											} /* DB   ED		  */
OP(ed,eb) { 											} /* DB   ED		  */
OP(ed,ec) { 											} /* DB   ED		  */
OP(ed,ed) { 											} /* DB   ED		  */
OP(ed,ee) { 											} /* DB   ED		  */
OP(ed,ef) { 											} /* DB   ED		  */

OP(ed,f0) { 											} /* DB   ED		  */
OP(ed,f1) { 											} /* DB   ED		  */
OP(ed,f2) { 											} /* DB   ED		  */
OP(ed,f3) { 											} /* DB   ED		  */
OP(ed,f4) { 											} /* DB   ED		  */
OP(ed,f5) { 											} /* DB   ED		  */
OP(ed,f6) { 											} /* DB   ED		  */
OP(ed,f7) { 											} /* DB   ED		  */

OP(ed,f8) { 											} /* DB   ED		  */
OP(ed,f9) { 											} /* DB   ED		  */
OP(ed,fa) { 											} /* DB   ED		  */
OP(ed,fb) { 											} /* DB   ED		  */
OP(ed,fc) { 											} /* DB   ED		  */
OP(ed,fd) { 											} /* DB   ED		  */
OP(ed,fe) { 											} /* DB   ED		  */
OP(ed,ff) { 											} /* DB   ED		  */

#define CHECK_BC_LOOP                                               \
if( _BC > 1 && _PCD < 0xfffc ) {									\
	UINT8 op1 = cpu_readop(_PCD);									\
	UINT8 op2 = cpu_readop(_PCD+1); 								\
	if( (op1==0x78 && op2==0xb1) || (op1==0x79 && op2==0xb0) )		\
	{																\
		UINT8 op3 = cpu_readop(_PCD+2); 							\
		UINT8 op4 = cpu_readop(_PCD+3); 							\
		if( op3==0x20 && op4==0xfb )								\
		{															\
			int cnt =												\
				cc[Z80_TABLE_op][0x78] +							\
				cc[Z80_TABLE_op][0xb1] +							\
				cc[Z80_TABLE_op][0x20] +							\
				cc[Z80_TABLE_ex][0x20]; 							\
			while( _BC > 0 && z80_ICount > cnt )					\
			{														\
				BURNODD( cnt, 4, cnt ); 							\
				_BC--;												\
			}														\
		}															\
		else														\
		if( op3 == 0xc2 )											\
		{															\
			UINT8 ad1 = cpu_readop_arg(_PCD+3); 					\
			UINT8 ad2 = cpu_readop_arg(_PCD+4); 					\
			if( (ad1 + 256 * ad2) == (_PCD - 1) )					\
			{														\
				int cnt =											\
					cc[Z80_TABLE_op][0x78] +						\
					cc[Z80_TABLE_op][0xb1] +						\
					cc[Z80_TABLE_op][0xc2] +						\
					cc[Z80_TABLE_ex][0xc2]; 						\
				while( _BC > 0 && z80_ICount > cnt )				\
				{													\
					BURNODD( cnt, 4, cnt ); 						\
					_BC--;											\
				}													\
			}														\
		}															\
	}																\
}

#define CHECK_DE_LOOP                                               \
if( _DE > 1 && _PCD < 0xfffc ) {                                    \
	UINT8 op1 = cpu_readop(_PCD);									\
	UINT8 op2 = cpu_readop(_PCD+1); 								\
	if( (op1==0x7a && op2==0xb3) || (op1==0x7b && op2==0xb2) )		\
	{																\
		UINT8 op3 = cpu_readop(_PCD+2); 							\
		UINT8 op4 = cpu_readop(_PCD+3); 							\
		if( op3==0x20 && op4==0xfb )								\
		{															\
			int cnt =												\
				cc[Z80_TABLE_op][0x7a] +							\
				cc[Z80_TABLE_op][0xb3] +							\
				cc[Z80_TABLE_op][0x20] +							\
                cc[Z80_TABLE_ex][0x20];                             \
			while( _DE > 0 && z80_ICount > cnt )					\
			{														\
				BURNODD( cnt, 4, cnt ); 							\
				_DE--;												\
			}														\
		}															\
		else														\
		if( op3==0xc2 ) 											\
		{															\
			UINT8 ad1 = cpu_readop_arg(_PCD+3); 					\
			UINT8 ad2 = cpu_readop_arg(_PCD+4); 					\
			if( (ad1 + 256 * ad2) == (_PCD - 1) )					\
			{														\
				int cnt =											\
					cc[Z80_TABLE_op][0x7a] +						\
					cc[Z80_TABLE_op][0xb3] +						\
					cc[Z80_TABLE_op][0xc2] +						\
                    cc[Z80_TABLE_ex][0xc2];                         \
				while( _DE > 0 && z80_ICount > cnt )				\
				{													\
					BURNODD( cnt, 4, cnt ); 						\
					_DE--;											\
				}													\
			}														\
		}															\
	}																\
}

#define CHECK_HL_LOOP                                               \
if( _HL > 1 && _PCD < 0xfffc ) {                                    \
	UINT8 op1 = cpu_readop(_PCD);									\
	UINT8 op2 = cpu_readop(_PCD+1); 								\
	if( (op1==0x7c && op2==0xb5) || (op1==0x7d && op2==0xb4) )		\
	{																\
		UINT8 op3 = cpu_readop(_PCD+2); 							\
		UINT8 op4 = cpu_readop(_PCD+3); 							\
		if( op3==0x20 && op4==0xfb )								\
		{															\
			int cnt =												\
				cc[Z80_TABLE_op][0x7c] +							\
				cc[Z80_TABLE_op][0xb5] +							\
				cc[Z80_TABLE_op][0x20] +							\
                cc[Z80_TABLE_ex][0x20];                             \
			while( _HL > 0 && z80_ICount > cnt )					\
			{														\
				BURNODD( cnt, 4, cnt ); 							\
				_HL--;												\
			}														\
		}															\
		else														\
		if( op3==0xc2 ) 											\
		{															\
			UINT8 ad1 = cpu_readop_arg(_PCD+3); 					\
			UINT8 ad2 = cpu_readop_arg(_PCD+4); 					\
			if( (ad1 + 256 * ad2) == (_PCD - 1) )					\
			{														\
				int cnt =											\
					cc[Z80_TABLE_op][0x7c] +						\
					cc[Z80_TABLE_op][0xb5] +						\
					cc[Z80_TABLE_op][0xc2] +						\
                    cc[Z80_TABLE_ex][0xc2];                         \
				while( _HL > 0 && z80_ICount > cnt )				\
				{													\
					BURNODD( cnt, 4, cnt ); 						\
					_HL--;											\
				}													\
			}														\
		}															\
	}																\
}

/**********************************************************
 * main opcodes
 **********************************************************/
OP(op,00) { 														} /* NOP			  */
OP(op,01) { _BC = ARG16();											} /* LD   BC,w		  */
OP(op,02) { WM( _BC, _A );											} /* LD   (BC),A	  */
OP(op,03) { _BC++;													} /* INC  BC		  */
OP(op,04) { _B = INC(_B);											} /* INC  B 		  */
OP(op,05) { _B = DEC(_B);											} /* DEC  B 		  */
OP(op,06) { _B = ARG(); 											} /* LD   B,n		  */
OP(op,07) { RLCA;													} /* RLCA			  */

OP(op,08) { EX_AF;													} /* EX   AF,AF'      */
OP(op,09) { ADD16(HL,BC);											} /* ADD  HL,BC 	  */
OP(op,0a) { _A = RM(_BC);											} /* LD   A,(BC)	  */
OP(op,0b) { _BC--; CHECK_BC_LOOP;									} /* DEC  BC		  */
OP(op,0c) { _C = INC(_C);											} /* INC  C 		  */
OP(op,0d) { _C = DEC(_C);											} /* DEC  C 		  */
OP(op,0e) { _C = ARG(); 											} /* LD   C,n		  */
OP(op,0f) { RRCA;													} /* RRCA			  */

OP(op,10) { _B--; JR_COND( _B, 0x10 );								} /* DJNZ o 		  */
OP(op,11) { _DE = ARG16();											} /* LD   DE,w		  */
OP(op,12) { WM( _DE, _A );											} /* LD   (DE),A	  */
OP(op,13) { _DE++;													} /* INC  DE		  */
OP(op,14) { _D = INC(_D);											} /* INC  D 		  */
OP(op,15) { _D = DEC(_D);											} /* DEC  D 		  */
OP(op,16) { _D = ARG(); 											} /* LD   D,n		  */
OP(op,17) { RLA;													} /* RLA			  */

OP(op,18) { JR();													} /* JR   o 		  */
OP(op,19) { ADD16(HL,DE);											} /* ADD  HL,DE 	  */
OP(op,1a) { _A = RM(_DE);											} /* LD   A,(DE)	  */
OP(op,1b) { _DE--; CHECK_DE_LOOP;									} /* DEC  DE		  */
OP(op,1c) { _E = INC(_E);											} /* INC  E 		  */
OP(op,1d) { _E = DEC(_E);											} /* DEC  E 		  */
OP(op,1e) { _E = ARG(); 											} /* LD   E,n		  */
OP(op,1f) { RRA;													} /* RRA			  */

OP(op,20) { JR_COND( !(_F & ZF), 0x20 );							} /* JR   NZ,o		  */
OP(op,21) { _HL = ARG16();											} /* LD   HL,w		  */
OP(op,22) { EA = ARG16(); WM16( EA, &Z80.HL );						} /* LD   (w),HL	  */
OP(op,23) { _HL++;													} /* INC  HL		  */
OP(op,24) { _H = INC(_H);											} /* INC  H 		  */
OP(op,25) { _H = DEC(_H);											} /* DEC  H 		  */
OP(op,26) { _H = ARG(); 											} /* LD   H,n		  */
OP(op,27) { DAA;													} /* DAA			  */

OP(op,28) { JR_COND( _F & ZF, 0x28 );								} /* JR   Z,o		  */
OP(op,29) { ADD16(HL,HL);											} /* ADD  HL,HL 	  */
OP(op,2a) { EA = ARG16(); RM16( EA, &Z80.HL );						} /* LD   HL,(w)	  */
OP(op,2b) { _HL--; CHECK_HL_LOOP;									} /* DEC  HL		  */
OP(op,2c) { _L = INC(_L);											} /* INC  L 		  */
OP(op,2d) { _L = DEC(_L);											} /* DEC  L 		  */
OP(op,2e) { _L = ARG(); 											} /* LD   L,n		  */
OP(op,2f) { _A ^= 0xff; _F = (_F&(SF|ZF|PF|CF))|HF|NF|(_A&(YF|XF)); } /* CPL			  */

OP(op,30) { JR_COND( !(_F & CF), 0x30 );							} /* JR   NC,o		  */
OP(op,31) { _SP = ARG16();											} /* LD   SP,w		  */
OP(op,32) { EA = ARG16(); WM( EA, _A ); 							} /* LD   (w),A 	  */
OP(op,33) { _SP++;													} /* INC  SP		  */
OP(op,34) { WM( _HL, INC(RM(_HL)) );								} /* INC  (HL)		  */
OP(op,35) { WM( _HL, DEC(RM(_HL)) );								} /* DEC  (HL)		  */
OP(op,36) { WM( _HL, ARG() );										} /* LD   (HL),n	  */
OP(op,37) { _F = (_F & (SF|ZF|PF)) | CF | (_A & (YF|XF));			} /* SCF			  */

OP(op,38) { JR_COND( _F & CF, 0x38 );								} /* JR   C,o		  */
OP(op,39) { ADD16(HL,SP);											} /* ADD  HL,SP 	  */
OP(op,3a) { EA = ARG16(); _A = RM( EA );							} /* LD   A,(w) 	  */
OP(op,3b) { _SP--;													} /* DEC  SP		  */
OP(op,3c) { _A = INC(_A);											} /* INC  A 		  */
OP(op,3d) { _A = DEC(_A);											} /* DEC  A 		  */
OP(op,3e) { _A = ARG(); 											} /* LD   A,n		  */
OP(op,3f) { _F = ((_F&(SF|ZF|PF|CF))|((_F&CF)<<4)|(_A&(YF|XF)))^CF; } /* CCF			  */

OP(op,40) { 														} /* LD   B,B		  */
OP(op,41) { _B = _C;												} /* LD   B,C		  */
OP(op,42) { _B = _D;												} /* LD   B,D		  */
OP(op,43) { _B = _E;												} /* LD   B,E		  */
OP(op,44) { _B = _H;												} /* LD   B,H		  */
OP(op,45) { _B = _L;												} /* LD   B,L		  */
OP(op,46) { _B = RM(_HL);											} /* LD   B,(HL)	  */
OP(op,47) { _B = _A;												} /* LD   B,A		  */

OP(op,48) { _C = _B;												} /* LD   C,B		  */
OP(op,49) { 														} /* LD   C,C		  */
OP(op,4a) { _C = _D;												} /* LD   C,D		  */
OP(op,4b) { _C = _E;												} /* LD   C,E		  */
OP(op,4c) { _C = _H;												} /* LD   C,H		  */
OP(op,4d) { _C = _L;												} /* LD   C,L		  */
OP(op,4e) { _C = RM(_HL);											} /* LD   C,(HL)	  */
OP(op,4f) { _C = _A;												} /* LD   C,A		  */

OP(op,50) { _D = _B;												} /* LD   D,B		  */
OP(op,51) { _D = _C;												} /* LD   D,C		  */
OP(op,52) { 														} /* LD   D,D		  */
OP(op,53) { _D = _E;												} /* LD   D,E		  */
OP(op,54) { _D = _H;												} /* LD   D,H		  */
OP(op,55) { _D = _L;												} /* LD   D,L		  */
OP(op,56) { _D = RM(_HL);											} /* LD   D,(HL)	  */
OP(op,57) { _D = _A;												} /* LD   D,A		  */

OP(op,58) { _E = _B;												} /* LD   E,B		  */
OP(op,59) { _E = _C;												} /* LD   E,C		  */
OP(op,5a) { _E = _D;												} /* LD   E,D		  */
OP(op,5b) { 														} /* LD   E,E		  */
OP(op,5c) { _E = _H;												} /* LD   E,H		  */
OP(op,5d) { _E = _L;												} /* LD   E,L		  */
OP(op,5e) { _E = RM(_HL);											} /* LD   E,(HL)	  */
OP(op,5f) { _E = _A;												} /* LD   E,A		  */

OP(op,60) { _H = _B;												} /* LD   H,B		  */
OP(op,61) { _H = _C;												} /* LD   H,C		  */
OP(op,62) { _H = _D;												} /* LD   H,D		  */
OP(op,63) { _H = _E;												} /* LD   H,E		  */
OP(op,64) { 														} /* LD   H,H		  */
OP(op,65) { _H = _L;												} /* LD   H,L		  */
OP(op,66) { _H = RM(_HL);											} /* LD   H,(HL)	  */
OP(op,67) { _H = _A;												} /* LD   H,A		  */

OP(op,68) { _L = _B;												} /* LD   L,B		  */
OP(op,69) { _L = _C;												} /* LD   L,C		  */
OP(op,6a) { _L = _D;												} /* LD   L,D		  */
OP(op,6b) { _L = _E;												} /* LD   L,E		  */
OP(op,6c) { _L = _H;												} /* LD   L,H		  */
OP(op,6d) { 														} /* LD   L,L		  */
OP(op,6e) { _L = RM(_HL);											} /* LD   L,(HL)	  */
OP(op,6f) { _L = _A;												} /* LD   L,A		  */

OP(op,70) { WM( _HL, _B );											} /* LD   (HL),B	  */
OP(op,71) { WM( _HL, _C );											} /* LD   (HL),C	  */
OP(op,72) { WM( _HL, _D );											} /* LD   (HL),D	  */
OP(op,73) { WM( _HL, _E );											} /* LD   (HL),E	  */
OP(op,74) { WM( _HL, _H );											} /* LD   (HL),H	  */
OP(op,75) { WM( _HL, _L );											} /* LD   (HL),L	  */
OP(op,76) { ENTER_HALT; 											} /* HALT			  */
OP(op,77) { WM( _HL, _A );											} /* LD   (HL),A	  */

OP(op,78) { _A = _B;												} /* LD   A,B		  */
OP(op,79) { _A = _C;												} /* LD   A,C		  */
OP(op,7a) { _A = _D;												} /* LD   A,D		  */
OP(op,7b) { _A = _E;												} /* LD   A,E		  */
OP(op,7c) { _A = _H;												} /* LD   A,H		  */
OP(op,7d) { _A = _L;												} /* LD   A,L		  */
OP(op,7e) { _A = RM(_HL);											} /* LD   A,(HL)	  */
OP(op,7f) { 														} /* LD   A,A		  */

OP(op,80) { ADD(_B);												} /* ADD  A,B		  */
OP(op,81) { ADD(_C);												} /* ADD  A,C		  */
OP(op,82) { ADD(_D);												} /* ADD  A,D		  */
OP(op,83) { ADD(_E);												} /* ADD  A,E		  */
OP(op,84) { ADD(_H);												} /* ADD  A,H		  */
OP(op,85) { ADD(_L);												} /* ADD  A,L		  */
OP(op,86) { ADD(RM(_HL));											} /* ADD  A,(HL)	  */
OP(op,87) { ADD(_A);												} /* ADD  A,A		  */

OP(op,88) { ADC(_B);												} /* ADC  A,B		  */
OP(op,89) { ADC(_C);												} /* ADC  A,C		  */
OP(op,8a) { ADC(_D);												} /* ADC  A,D		  */
OP(op,8b) { ADC(_E);												} /* ADC  A,E		  */
OP(op,8c) { ADC(_H);												} /* ADC  A,H		  */
OP(op,8d) { ADC(_L);												} /* ADC  A,L		  */
OP(op,8e) { ADC(RM(_HL));											} /* ADC  A,(HL)	  */
OP(op,8f) { ADC(_A);												} /* ADC  A,A		  */

OP(op,90) { SUB(_B);												} /* SUB  B 		  */
OP(op,91) { SUB(_C);												} /* SUB  C 		  */
OP(op,92) { SUB(_D);												} /* SUB  D 		  */
OP(op,93) { SUB(_E);												} /* SUB  E 		  */
OP(op,94) { SUB(_H);												} /* SUB  H 		  */
OP(op,95) { SUB(_L);												} /* SUB  L 		  */
OP(op,96) { SUB(RM(_HL));											} /* SUB  (HL)		  */
OP(op,97) { SUB(_A);												} /* SUB  A 		  */

OP(op,98) { SBC(_B);												} /* SBC  A,B		  */
OP(op,99) { SBC(_C);												} /* SBC  A,C		  */
OP(op,9a) { SBC(_D);												} /* SBC  A,D		  */
OP(op,9b) { SBC(_E);												} /* SBC  A,E		  */
OP(op,9c) { SBC(_H);												} /* SBC  A,H		  */
OP(op,9d) { SBC(_L);												} /* SBC  A,L		  */
OP(op,9e) { SBC(RM(_HL));											} /* SBC  A,(HL)	  */
OP(op,9f) { SBC(_A);												} /* SBC  A,A		  */

OP(op,a0) { AND(_B);												} /* AND  B 		  */
OP(op,a1) { AND(_C);												} /* AND  C 		  */
OP(op,a2) { AND(_D);												} /* AND  D 		  */
OP(op,a3) { AND(_E);												} /* AND  E 		  */
OP(op,a4) { AND(_H);												} /* AND  H 		  */
OP(op,a5) { AND(_L);												} /* AND  L 		  */
OP(op,a6) { AND(RM(_HL));											} /* AND  (HL)		  */
OP(op,a7) { AND(_A);												} /* AND  A 		  */

OP(op,a8) { XOR(_B);												} /* XOR  B 		  */
OP(op,a9) { XOR(_C);												} /* XOR  C 		  */
OP(op,aa) { XOR(_D);												} /* XOR  D 		  */
OP(op,ab) { XOR(_E);												} /* XOR  E 		  */
OP(op,ac) { XOR(_H);												} /* XOR  H 		  */
OP(op,ad) { XOR(_L);												} /* XOR  L 		  */
OP(op,ae) { XOR(RM(_HL));											} /* XOR  (HL)		  */
OP(op,af) { XOR(_A);												} /* XOR  A 		  */

OP(op,b0) { OR(_B); 												} /* OR   B 		  */
OP(op,b1) { OR(_C); 												} /* OR   C 		  */
OP(op,b2) { OR(_D); 												} /* OR   D 		  */
OP(op,b3) { OR(_E); 												} /* OR   E 		  */
OP(op,b4) { OR(_H); 												} /* OR   H 		  */
OP(op,b5) { OR(_L); 												} /* OR   L 		  */
OP(op,b6) { OR(RM(_HL));											} /* OR   (HL)		  */
OP(op,b7) { OR(_A); 												} /* OR   A 		  */

OP(op,b8) { CP(_B); 												} /* CP   B 		  */
OP(op,b9) { CP(_C); 												} /* CP   C 		  */
OP(op,ba) { CP(_D); 												} /* CP   D 		  */
OP(op,bb) { CP(_E); 												} /* CP   E 		  */
OP(op,bc) { CP(_H); 												} /* CP   H 		  */
OP(op,bd) { CP(_L); 												} /* CP   L 		  */
OP(op,be) { CP(RM(_HL));											} /* CP   (HL)		  */
OP(op,bf) { CP(_A); 												} /* CP   A 		  */

OP(op,c0) { RET_COND( !(_F & ZF), 0xc0 );							} /* RET  NZ		  */
OP(op,c1) { POP(BC);												} /* POP  BC		  */
OP(op,c2) { JP_COND( !(_F & ZF) );									} /* JP   NZ,a		  */
OP(op,c3) { JP; 													} /* JP   a 		  */
OP(op,c4) { CALL_COND( !(_F & ZF), 0xc4 );							} /* CALL NZ,a		  */
OP(op,c5) { PUSH( BC ); 											} /* PUSH BC		  */
OP(op,c6) { ADD(ARG()); 											} /* ADD  A,n		  */
OP(op,c7) { RST(0x00);												} /* RST  0 		  */

OP(op,c8) { RET_COND( _F & ZF, 0xc8 );								} /* RET  Z 		  */
OP(op,c9) { POP(PC); change_pc16(_PCD); 							} /* RET			  */
OP(op,ca) { JP_COND( _F & ZF ); 									} /* JP   Z,a		  */
OP(op,cb) { _R++; EXEC(cb,ROP());									} /* **** CB xx 	  */
OP(op,cc) { CALL_COND( _F & ZF, 0xcc ); 							} /* CALL Z,a		  */
OP(op,cd) { CALL(); 												} /* CALL a 		  */
OP(op,ce) { ADC(ARG()); 											} /* ADC  A,n		  */
OP(op,cf) { RST(0x08);												} /* RST  1 		  */

OP(op,d0) { RET_COND( !(_F & CF), 0xd0 );							} /* RET  NC		  */
OP(op,d1) { POP(DE);												} /* POP  DE		  */
OP(op,d2) { JP_COND( !(_F & CF) );									} /* JP   NC,a		  */
OP(op,d3) { unsigned n = ARG() | (_A << 8); OUT( n, _A );			} /* OUT  (n),A 	  */
OP(op,d4) { CALL_COND( !(_F & CF), 0xd4 );							} /* CALL NC,a		  */
OP(op,d5) { PUSH( DE ); 											} /* PUSH DE		  */
OP(op,d6) { SUB(ARG()); 											} /* SUB  n 		  */
OP(op,d7) { RST(0x10);												} /* RST  2 		  */

OP(op,d8) { RET_COND( _F & CF, 0xd8 );								} /* RET  C 		  */
OP(op,d9) { EXX;													} /* EXX			  */
OP(op,da) { JP_COND( _F & CF ); 									} /* JP   C,a		  */
OP(op,db) { unsigned n = ARG() | (_A << 8); _A = IN( n );			} /* IN   A,(n) 	  */
OP(op,dc) { CALL_COND( _F & CF, 0xdc ); 							} /* CALL C,a		  */
OP(op,dd) { _R++; EXEC(dd,ROP());									} /* **** DD xx 	  */
OP(op,de) { SBC(ARG()); 											} /* SBC  A,n		  */
OP(op,df) { RST(0x18);												} /* RST  3 		  */

OP(op,e0) { RET_COND( !(_F & PF), 0xe0 );							} /* RET  PO		  */
OP(op,e1) { POP(HL);												} /* POP  HL		  */
OP(op,e2) { JP_COND( !(_F & PF) );									} /* JP   PO,a		  */
OP(op,e3) { EXSP(HL);												} /* EX   HL,(SP)	  */
OP(op,e4) { CALL_COND( !(_F & PF), 0xe4 );							} /* CALL PO,a		  */
OP(op,e5) { PUSH( HL ); 											} /* PUSH HL		  */
OP(op,e6) { AND(ARG()); 											} /* AND  n 		  */
OP(op,e7) { RST(0x20);												} /* RST  4 		  */

OP(op,e8) { RET_COND( _F & PF, 0xe8 );								} /* RET  PE		  */
OP(op,e9) { _PC = _HL; change_pc16(_PCD);							} /* JP   (HL)		  */
OP(op,ea) { JP_COND( _F & PF ); 									} /* JP   PE,a		  */
OP(op,eb) { EX_DE_HL;												} /* EX   DE,HL 	  */
OP(op,ec) { CALL_COND( _F & PF, 0xec ); 							} /* CALL PE,a		  */
OP(op,ed) { _R++; EXEC(ed,ROP());									} /* **** ED xx 	  */
OP(op,ee) { XOR(ARG()); 											} /* XOR  n 		  */
OP(op,ef) { RST(0x28);												} /* RST  5 		  */

OP(op,f0) { RET_COND( !(_F & SF), 0xf0 );							} /* RET  P 		  */
OP(op,f1) { POP(AF);												} /* POP  AF		  */
OP(op,f2) { JP_COND( !(_F & SF) );									} /* JP   P,a		  */
OP(op,f3) { _IFF1 = _IFF2 = 0;										} /* DI 			  */
OP(op,f4) { CALL_COND( !(_F & SF), 0xf4 );							} /* CALL P,a		  */
OP(op,f5) { PUSH( AF ); 											} /* PUSH AF		  */
OP(op,f6) { OR(ARG());												} /* OR   n 		  */
OP(op,f7) { RST(0x30);												} /* RST  6 		  */

OP(op,f8) { RET_COND( _F & SF, 0xf8 );								} /* RET  M 		  */
OP(op,f9) { _SP = _HL;												} /* LD   SP,HL 	  */
OP(op,fa) { JP_COND(_F & SF);										} /* JP   M,a		  */
OP(op,fb) { EI; 													} /* EI 			  */
OP(op,fc) { CALL_COND( _F & SF, 0xfc ); 							} /* CALL M,a		  */
OP(op,fd) { _R++; EXEC(fd,ROP());									} /* **** FD xx 	  */
OP(op,fe) { CP(ARG());												} /* CP   n 		  */
OP(op,ff) { RST(0x38);												} /* RST  7 		  */
