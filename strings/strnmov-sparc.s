! Copyright (C) 2000, 2002 MySQL AB
!  All rights reserved. Use is subject to license terms.
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; version 2 of the License.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License
! along with this program; if not, write to the Free Software
! Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
!
!

	.file	"strnmov-sparc.s"
.section	".text"
	.align 4
	.global strnmov
	.type	 strnmov,#function
	.proc	0102
strnmov:
	orcc	%g0,%o2,%g0
	be,a	.end
	nop
	ldsb	[%o1],%o3
.loop:	
	stb	%o3,[%o0]
	cmp	%o3,0
	be	.end		! Jump to end on end of string
	add	%o1,1,%o1
	add	%o0,1,%o0
	subcc	%o2,1,%o2
	bne,a	.loop
	ldsb	[%o1],%o3
.end:
	retl
	nop
.strnmov_end:
	.size	 strnmov,.strnmov_end-strnmov
	.ident	"Matt Wagner"
