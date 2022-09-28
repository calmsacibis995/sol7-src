/*
 *	Copyright (c) 1997 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)audit.c	1.22	97/11/25 SMI"

#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/lwp.h>
#include	<stdio.h>
#include	<stdarg.h>
#include	<dlfcn.h>
#include	<string.h>
#include	"debug.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"

const char *		audit_libraries;
uint_t			audit_arg_count = 64;	/* number of args to copy */
						/*	when dupping a */
						/*	stack in auditing. */
						/*	default == all */
static List 		audit_liblist = {0, 0};
static uint_t		audit_libcount = 0;	/* cnt. of auditing libraries */

/* LINTLIBRARY */
uint_t			audit_flags = 0;

static Addr
audit_symget(Rt_map * lmp, const char * name, const char * audlibname)
{
	Rt_map *	_lmp;
	Sym *		sym;
	Addr		addr;
	Slookup		sl;

	sl.sl_name = name;
	sl.sl_permit = 0;
	sl.sl_cmap = lml_rtld.lm_head;
	sl.sl_imap = lmp;
	sl.sl_rsymndx = 0;
	if ((sym = LM_LOOKUP_SYM(lmp)(&sl, &_lmp,
	    (LKUP_DEFT | LKUP_FIRST))) == (Sym *) 0) {
		return (0);
	}
	addr = sym->st_value;
	if (!(FLAGS(lmp) & FLG_RT_FIXED))
		addr += ADDR(lmp);

	DBG_CALL(Dbg_audit_bound(audlibname, name));
	return (addr);
}


int
audit_objopen(Rt_map * lmp)
{
	Audit_library *	alp;
	Audit_fields *	afp;
	Listnode *	lnp;
	Lmid_t		lmid;
	int		ndx = 0;
	int		applic = 0;

	/*
	 * The initial allocation of the AUDIT field includes space
	 * for:
	 *	Audit_fields
	 *		audit_libinfo[audit_libcount]
	 */
	if ((AUDIT(lmp) = afp = calloc(1, sizeof (Audit_fields) +
	    (sizeof (Audit_libinfo) * audit_libcount))) == 0)
		return (0);

	afp->af_libinfo = (Audit_libinfo *)((uintptr_t)afp +
		sizeof (Audit_fields));

	lmid = get_linkmap_id(LIST(lmp));

	if ((rtld_flags & RT_FL_APPLIC) == 0) {
		applic++;
		rtld_flags |= RT_FL_APPLIC;
	}

	for (LIST_TRAVERSE(&audit_liblist, lnp, alp)) {
		uint_t		retval;
		Audit_libinfo *	aip;

		FLAGS(lmp) |= FLG_RT_AUDIT;
		aip = &afp->af_libinfo[ndx];

		aip->ai_cookie = (uintptr_t)lmp;

		if (!alp->al_objopen) {
			ndx++;
			continue;
		}
		retval = (*alp->al_objopen)((Link_map *)lmp, lmid,
			&(aip->ai_cookie));
		if (retval & LA_FLG_BINDTO)
			aip->ai_flags |= FLG_AI_BINDTO;

		if (retval & LA_FLG_BINDFROM) {
			/*
			 * we only need a dynamic plt's if a pltenter
			 * and/or a pltexit() entry point exist in one
			 * of our auditing libraries.
			 */

			if (!afp->af_dynplts && JMPREL(lmp) && (audit_flags &
			    (AF_PLTENTER | AF_PLTEXIT))) {
				ulong_t		pltcnt;
				/*
				 * We create one dynplt for every 'PLT' that
				 * exists in the object.
				 */
				pltcnt = PLTRELSZ(lmp) / RELENT(lmp);
				if ((afp->af_dynplts = calloc(pltcnt,
				    dyn_plt_ent_size)) == 0)
					return (0);
			}
			aip->ai_flags |= FLG_AI_BINDFROM;
		}


		DBG_CALL(Dbg_audit_libaudit(NAME(lmp)));
		ndx++;
	}
	if (applic)
		rtld_flags &= ~RT_FL_APPLIC;

	return (1);
}

void
audit_objclose(Rt_map * lmp)
{
	Audit_library *	alp;
	Listnode *	lnp;
	int		ndx = 0;
	int		applic = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0) {
		applic++;
		rtld_flags |= RT_FL_APPLIC;
	}
	for (LIST_TRAVERSE(&audit_liblist, lnp, alp)) {
		if (alp->al_objclose)
			(*alp->al_objclose)(
			    &(AUDIT(lmp)->af_libinfo[ndx].ai_cookie));
		ndx++;
	}
	if (applic)
		rtld_flags &= ~RT_FL_APPLIC;
}

Addr
audit_pltenter(Rt_map * rlmp, Rt_map * dlmp, Sym * symp, uint_t symndx,
	void * regset, uint_t * sb_flags)
{
	Audit_library *	alp;
	Listnode *	lnp;
	int		ndx = 0;
	Addr		ret = symp->st_value;
	int		applic = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0) {
		applic++;
		rtld_flags |= RT_FL_APPLIC;
	}

	for (LIST_TRAVERSE(&audit_liblist, lnp, alp)) {
		Audit_libinfo *	refaip;
		Audit_libinfo *	defaip;

		refaip = &AUDIT(rlmp)->af_libinfo[ndx];
		defaip = &AUDIT(dlmp)->af_libinfo[ndx];

		if (alp->al_pltenter &&
		    (refaip->ai_flags & FLG_AI_BINDFROM) &&
		    (defaip->ai_flags & FLG_AI_BINDTO)) {
			DBG_CALL(Dbg_audit_pltenter(symp,
			    symp->st_name + STRTAB(dlmp), alp->al_libname));
			ret = (Addr)(*alp->al_pltenter)(symp, symndx,
				&(AUDIT(rlmp)->af_libinfo[ndx].ai_cookie),
				&(AUDIT(dlmp)->af_libinfo[ndx].ai_cookie),
#if	defined(_LP64)
				regset, sb_flags,
				symp->st_name + STRTAB(dlmp));
#else
				regset, sb_flags);
#endif
			if (ret != symp->st_value) {
				DBG_CALL(Dbg_audit_pltbind(symp,
					symp->st_name + STRTAB(dlmp),
					alp->al_libname, ret));
			}
		}
		ndx++;
	}

	if (applic)
		rtld_flags &= ~RT_FL_APPLIC;

	return (ret);
}


Addr
audit_pltexit(uintptr_t retval, Rt_map * rlmp, Rt_map * dlmp, Sym * symp,
	uint_t symndx)
{
	Audit_library *	alp;
	Listnode *	lnp;
	int		ndx = 0;
	uintptr_t	ret = retval;
	int		applic = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0) {
		applic++;
		rtld_flags |= RT_FL_APPLIC;
	}


	for (LIST_TRAVERSE(&audit_liblist, lnp, alp)) {
		Audit_libinfo *	refaip;
		Audit_libinfo *	defaip;

		refaip = &AUDIT(rlmp)->af_libinfo[ndx];
		defaip = &AUDIT(dlmp)->af_libinfo[ndx];

		if (alp->al_pltexit &&
		    (refaip->ai_flags & FLG_AI_BINDFROM) &&
		    (defaip->ai_flags & FLG_AI_BINDTO)) {
			ret = (*alp->al_pltexit)(symp, symndx,
				&(AUDIT(rlmp)->af_libinfo[ndx].ai_cookie),
				&(AUDIT(dlmp)->af_libinfo[ndx].ai_cookie),
#if	defined(_LP64)
				retval, symp->st_name + STRTAB(dlmp));
#else
				retval);
#endif
		}
		ndx++;
	}

	if (applic)
		rtld_flags &= ~RT_FL_APPLIC;

	return (ret);
}

Addr
audit_symbind(Sym * symp, uint_t symndx, Rt_map * reflm,
	Rt_map * deflm, Addr value, uint_t * flags)
{
	Audit_library * alp;
	Listnode *	lnp;
	int		ndx = 0;
	Sym		sym;
	int		applic = 0;

	/*
	 * Quick shortcut to get out of here if nothing will happen.
	 */
	if (!(audit_flags & AF_SYMBIND))
		return (value);

	if ((rtld_flags & RT_FL_APPLIC) == 0) {
		applic++;
		rtld_flags |= RT_FL_APPLIC;
	}

	sym = *symp;

	sym.st_value = value;

#if	!defined(_LP64)
	/*
	 * In the 64-bit world the st_name field is only 32-bits
	 * which isn't big enough to hold a charater pointer. We
	 * pass this pointer as a separate parameter for 64-bit
	 * audit libraries.
	 */
	sym.st_name += (Word)STRTAB(deflm);
#endif

	for (LIST_TRAVERSE(&audit_liblist, lnp, alp)) {
		Audit_libinfo *	refaip;
		Audit_libinfo *	defaip;
		Addr		prevvalue;

		DBG_CALL(Dbg_audit_symbind(alp->al_libname,
			(const char *)STRTAB(deflm), symndx));
		refaip = &AUDIT(reflm)->af_libinfo[ndx];
		defaip = &AUDIT(deflm)->af_libinfo[ndx];

		if ((refaip->ai_flags & FLG_AI_BINDFROM) &&
		    (defaip->ai_flags & FLG_AI_BINDTO) &&
		    alp->al_symbind) {
				prevvalue = sym.st_value;
				sym.st_value = (*alp->al_symbind)(&sym,
					symndx, &(refaip->ai_cookie),
#if	defined(_LP64)
					&(defaip->ai_cookie), flags,
					sym.st_name + STRTAB(deflm));
#else
					&(defaip->ai_cookie), flags);
#endif
				if (prevvalue != sym.st_value) {
					if (alp->al_vernum >= LAV_VERSION2)
						*flags |= LA_SYMB_ALTVALUE;
					DBG_CALL(Dbg_audit_symnewval(
						(const char *)STRTAB(deflm),
						alp->al_libname, prevvalue,
						sym.st_value));
				}
		}
		ndx++;
	}

	if (applic)
		rtld_flags &= ~RT_FL_APPLIC;

	return (sym.st_value);
}

void
audit_preinit(Rt_map * lmp)
{
	Audit_library *	alp;
	Listnode *	lnp;
	int		ndx = 0;
	int		applic = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0) {
		applic++;
		rtld_flags |= RT_FL_APPLIC;
	}

	for (LIST_TRAVERSE(&audit_liblist, lnp, alp)) {
		if (alp->al_preinit)
			(*alp->al_preinit)(
				&(AUDIT(lmp)->af_libinfo[ndx].ai_cookie));
		ndx++;
	}
	if (applic)
		rtld_flags &= ~RT_FL_APPLIC;
}

/*
 * Load the tracelibrary onto it's own private link-map and
 * find which 'interesting' entry points are present.
 */
int
audit_setup(const char * liblist)
{
	char *		libstr;
	char *		ptr;
	char *		ptr2;

	libstr = strdup(liblist);


	DBG_CALL(Dbg_audit_init(liblist));

	for (ptr = libstr; *ptr; ptr = ptr2) {
		Dl_obj *	dlp;
		Rt_map *	so_lmp;
		Rt_map **	tobj;
		Audit_library *	alp;
		const char *	pltenterstr;

		for (ptr2 = ptr; *ptr2 && (*ptr2 != ':'); ptr2++)
			;

		if (*ptr2 == ':') {
			*ptr2 = '\0';
			if (ptr2++ == ptr)
				continue;
		}

		/*
		 * If this is a secure application only allow
		 * simple filenames to be specified for auditing libraries.
		 * The lookup for these files will be restricted, but is
		 * allowed by placing auditing objects in secure directories.
		 */
		if (rtld_flags & RT_FL_SECURE) {
			if (strchr(ptr, '/')) {
				DBG_CALL(Dbg_libs_ignore(ptr));
				continue;
			}
		}


		if ((dlp = (Dl_obj*)dlmopen_core((Lm_list *)LM_ID_NEWLM, ptr,
		    RTLD_GLOBAL | RTLD_WORLD, &so_lmp,
		    lml_rtld.lm_head, LML_FLG_NOAUDIT)) == (Dl_obj *)0) {
			if (!(rtld_flags & RT_FL_DELINPROG) &&
			    (rtld_flags & RT_FL_DELNEEDED))
				remove_so();
			eprintf(ERR_WARNING, MSG_INTL(MSG_AUD_NOOPEN), ptr);
			continue;
		}

		if ((alp = (Audit_library *)calloc(1,
		    sizeof (Audit_library))) == 0)
			return (0);

		alp->al_libname = ptr;
		alp->al_dlp = dlp;

		if ((alp->al_version =
		    (uint_t(*)(uint_t))audit_symget(so_lmp,
		    MSG_ORIG(MSG_SYM_LAVERSION), alp->al_libname)) == 0) {
			eprintf(ERR_WARNING, MSG_INTL(MSG_AUD_REQSYM),
				MSG_ORIG(MSG_SYM_LAVERSION), ptr);
			free(alp);
			(void) dlclose_core(so_lmp, dlp);
			if (!(rtld_flags & RT_FL_DELINPROG) &&
			    (rtld_flags & RT_FL_DELNEEDED))
				remove_so();
			continue;
		}


		alp->al_preinit = (void(*)(uintptr_t *))audit_symget(so_lmp,
			MSG_ORIG(MSG_SYM_LAPREINIT), alp->al_libname);

		alp->al_objopen = (uint_t(*)(Link_map *, Lmid_t,
			uintptr_t *))audit_symget(so_lmp,
			MSG_ORIG(MSG_SYM_LAOBJOPEN), alp->al_libname);

		alp->al_objclose = (uint_t(*)(uintptr_t *))
			audit_symget(so_lmp, MSG_ORIG(MSG_SYM_LAOBJCLOSE),
				alp->al_libname);

#if	defined(_LP64)
		if (alp->al_symbind = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uint_t *, const char *))
		    audit_symget(so_lmp, MSG_ORIG(MSG_SYM_LASYMBIND64),
		    alp->al_libname))
			audit_flags |= AF_SYMBIND;
#else
		if (alp->al_symbind = (uintptr_t(*)(Sym *,
		    uint_t, uintptr_t *, uintptr_t *, uint_t *))
		    audit_symget(so_lmp, MSG_ORIG(MSG_SYM_LASYMBIND),
		    alp->al_libname))
			audit_flags |= AF_SYMBIND;
#endif



#if	defined(__sparcv9)
		pltenterstr = MSG_ORIG(MSG_SYM_LAV9PLTENTER);
#elif	defined(__sparc)
		pltenterstr = MSG_ORIG(MSG_SYM_LAV8PLTENTER);
#else
		pltenterstr = MSG_ORIG(MSG_SYM_LAX86PLTENTER);
#endif


#if	defined(_LP64)
		if (alp->al_pltenter = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, void *, uint_t *, const char *))
		    audit_symget(so_lmp, pltenterstr, alp->al_libname))
			audit_flags |= AF_PLTENTER;
#else
		if (alp->al_pltenter = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, void *, uint_t *))
		    audit_symget(so_lmp, pltenterstr, alp->al_libname))
			audit_flags |= AF_PLTENTER;
#endif


#if	defined(_LP64)
		if (alp->al_pltexit = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uintptr_t, const char *))
		    audit_symget(so_lmp, MSG_ORIG(MSG_SYM_LAPLTEXIT64),
		    alp->al_libname))
			audit_flags |= AF_PLTEXIT;
#else
		if (alp->al_pltexit = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uintptr_t)) audit_symget(so_lmp,
		    MSG_ORIG(MSG_SYM_LAPLTEXIT), alp->al_libname))
			audit_flags |= AF_PLTEXIT;
#endif

		if ((tobj = tsort(so_lmp, RT_SORT_REV)) ==
		    (Rt_map **)S_ERROR)
			return (0);
		rtld_flags |= RT_FL_APPLIC;
		if (tobj != (Rt_map **)NULL)
			call_init(tobj);
		rtld_flags &= ~RT_FL_APPLIC;

		if (alp->al_version) {
			uint_t		retver;

			rtld_flags |= RT_FL_APPLIC;

			retver = alp->al_version(LAV_CURRENT);

			rtld_flags &= ~RT_FL_APPLIC;

			if ((retver < LAV_VERSION1) ||
			    (retver > LAV_CURRENT)) {
				DBG_CALL(Dbg_audit_badvers(ptr, retver));
				(void) dlclose_core(so_lmp, dlp);
				if (!(rtld_flags & RT_FL_DELINPROG) &&
				    (rtld_flags & RT_FL_DELNEEDED))
				remove_so();
				free(alp);
				continue;
			}
			alp->al_vernum = retver;
			DBG_CALL(Dbg_audit_version(ptr, retver));
		}
		if (list_append(&audit_liblist, alp) == 0)
			return (0);
		audit_libcount++;
		rtld_flags |= RT_FL_AUDITING;
	}



	/*
	 * call la_objopen for the primary objects which
	 * should already be loaded on the link-map at this point.
	 */
	if (audit_objopen(lml_main.lm_head) == 0)
		return (0);
	if (audit_objopen(lml_rtld.lm_head) == 0)
		return (0);
	return (1);
}
