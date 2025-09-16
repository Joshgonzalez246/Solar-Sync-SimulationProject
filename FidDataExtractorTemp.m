%% Minimal Pmpp(T) extractor using Vmp ordering @ 1000 W/m^2
fig = gcf;
axs = findall(fig,'Type','axes');

% Find the P–V axes by Y-label containing 'Power'
ylabs = arrayfun(@(a) string(get(get(a,'YLabel'),'String')), axs, 'UniformOutput', true);
idxPV = find(contains(ylabs,"Power",'IgnoreCase',true),1);
assert(~isempty(idxPV),'Could not find P–V axes.');
axPV = axs(idxPV);

% All line objects on P–V axes
lns = findobj(axPV,'Type','line');
assert(~isempty(lns),'No line objects found on the P–V axes.');

% Keep only continuous curves (ignore marker-only lines)
lns = lns(arrayfun(@(h) numel(get(h,'XData')), lns) > 20);

% For each curve, find Pmax and the corresponding V at Pmax (Vmp)
Pmax  = zeros(1,numel(lns));
Vmp   = zeros(1,numel(lns));
for k = 1:numel(lns)
    V = get(lns(k),'XData');
    P = get(lns(k),'YData');
    [Pmax(k), idx] = max(P);
    Vmp(k) = V(idx);
end

% Collapse exact duplicates (some releases draw each curve twice)
[~, ia] = unique([round(Pmax,9); round(Vmp,9)].','rows','stable');
Pmax = Pmax(ia); Vmp = Vmp(ia);

% >>> EDIT: temperatures you asked the PV dialog to plot (°C), ascending
T_grid = [0 25 50 75];

% Order curves by Vmp: colder → higher Vmp. So ascending T corresponds to DESCENDING Vmp.
[~, orderByVmp]   = sort(Vmp, 'descend');   % high Vmp = low T
Pmpp_1000_vs_T    = Pmax(orderByVmp);

% Length guard (truncate if mismatch)
n = min(numel(T_grid), numel(Pmpp_1000_vs_T));
T_grid           = T_grid(1:n);
Pmpp_1000_vs_T   = Pmpp_1000_vs_T(1:n);

save pmpp_1000_vs_T.mat T_grid Pmpp_1000_vs_T

% Quick plot
figure; plot(T_grid, Pmpp_1000_vs_T, 'o-'); grid on
xlabel('Temperature (°C)'); ylabel('P_{MPP} @ 1000 W/m^2 (W)');
title('Extracted P_{MPP}(T) with V_{mpp}-based ordering');
