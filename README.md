<class Molecule:
    def __init__(self, formula):
        self.formula = formula

    def __str__(self):
        return self.formula

def hydrogenation(alkene, hydrogen):
    if alkene.formula == "C2H4" and hydrogen.formula == "H2":
        return Molecule("C2H6")
    else:
        raise ValueError("Reagentes incorretos para a hidrogenação.")

# Reagentes
eteno = Molecule("C2H4")
hidrogenio = Molecule("H2")

# Reação de Hidrogenação
try:
    etano = hydrogenation(eteno, hidrogenio)
    print(f"Produto da reação: {etano}")
except ValueError as e:
    print(e)>